/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "playlistclip.h"
#include "audio/audioInfo.h"
#include "bin.h"
#include "clipcreator.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "jobs/audiolevels/audiolevelstask.h"
#include "jobs/cachetask.h"
#include "jobs/cliploadtask.h"
#include "jobs/proxytask.h"
#include "kdenlivesettings.h"
#include "lib/audio/audioStreamInfo.h"
#include "macros.hpp"
#include "mltcontroller/clippropertiescontroller.h"
#include "model/markerlistmodel.hpp"
#include "model/markersortmodel.h"
#include "playlistsubclip.h"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "projectfolder.h"
#include "projectitemmodel.h"
#include "projectsubclip.h"
#include "timeline2/model/snapmodel.hpp"
#include "utils/thumbnailcache.hpp"
#include "utils/timecode.h"
#include "xml/xml.hpp"

#include "kdenlive_debug.h"
#include <KIO/RenameDialog>
#include <KImageCache>
#include <KLocalizedString>
#include <KMessageBox>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDomElement>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QPainter>
#include <QProcess>
#include <QtMath>

#ifdef CRASH_AUTO_TEST
#include "logger.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <rttr/registration>

#pragma GCC diagnostic pop
RTTR_REGISTRATION
{
    using namespace rttr;
    registration::class_<PlaylistClip>("PlaylistClip");
}
#endif

PlaylistClip::PlaylistClip(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model, std::shared_ptr<Mlt::Producer> &producer)
    : ProjectClip(id, thumb, model, producer)
{
    // Read some basic infos from the playlist
    m_clipType = ClipType::Playlist;
    parsePlaylistProps();
}

// static
std::shared_ptr<PlaylistClip> PlaylistClip::construct(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model,
                                                      std::shared_ptr<Mlt::Producer> &producer)
{
    std::shared_ptr<PlaylistClip> self(new PlaylistClip(id, thumb, model, producer));
    baseFinishConstruct(self);
    QMetaObject::invokeMethod(model.get(), "loadSubClips", Qt::QueuedConnection, Q_ARG(QString, id),
                              Q_ARG(QString, self->getProducerProperty(QStringLiteral("kdenlive:clipzones"))), Q_ARG(bool, false));
    return self;
}

PlaylistClip::PlaylistClip(const QString &id, const QDomElement &description, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model)
    : ProjectClip(id, description, thumb, model)
{
}

std::shared_ptr<PlaylistClip> PlaylistClip::construct(const QString &id, const QDomElement &description, const QIcon &thumb,
                                                      std::shared_ptr<ProjectItemModel> model)
{
    std::shared_ptr<PlaylistClip> self(new PlaylistClip(id, description, thumb, std::move(model)));
    baseFinishConstruct(self);
    return self;
}

PlaylistClip::~PlaylistClip() {}

QDomElement PlaylistClip::toXml(QDomDocument &document, bool includeMeta, bool includeProfile)
{
    getProducerXML(document, includeMeta, includeProfile);
    QDomElement prod;
    prod = document.documentElement();
    prod.setAttribute(QStringLiteral("type"), int(m_clipType));
    return prod;
}

bool PlaylistClip::setProducer(std::shared_ptr<Mlt::Producer> producer, bool generateThumb, bool clearTrackProducers)
{
    // Get number of sequences in the project
    bool result = ProjectClip::setProducer(producer, generateThumb, clearTrackProducers);
    parsePlaylistProps();
    return result;
}

void PlaylistClip::parsePlaylistProps()
{
    if (!m_masterProducer) {
        return;
    }
    Mlt::Service s(m_masterProducer->parent());
    Mlt::Properties retainList(mlt_properties(s.get_data("xml_retain")));
    if (retainList.is_valid()) {
        qDebug() << "::::::::::\nRETAIN LIST VALID\n\n::";
        Mlt::Playlist playlist(mlt_playlist(retainList.get_data("main_bin")));
        // Get active timeline
        m_activeTimeline = QUuid(playlist.get("kdenlive:docproperties.activetimeline"));
        // Read project file properties
        QMap<QString, QString> parsableProperties;
        parsableProperties.insert(i18n("version"), QStringLiteral("kdenlive:docproperties.kdenliveversion"));
        QMapIterator<QString, QString> ix(parsableProperties);
        while (ix.hasNext()) {
            ix.next();
            if (playlist.property_exists(ix.value().toUtf8().constData())) {
                m_extraProperties.insert(ix.key(), qstrdup(playlist.get(ix.value().toUtf8().constData())));
            }
        }
        int max = playlist.count();
        m_sequences.clear();
        for (int i = 0; i < max; i++) {
            QScopedPointer<Mlt::Producer> prod(playlist.get_clip(i));
            if (prod->is_blank() || !prod->is_valid()) {
                continue;
            }
            if (prod->parent().type() == mlt_service_tractor_type) {
                if (prod->parent().property_exists("kdenlive:uuid")) {
                    // Load sequence properties
                    SequenceInfo info;
                    Mlt::Properties props(prod->parent());
                    const QUuid sequenceUuid(props.get("kdenlive:uuid"));
                    info.sequenceName = qstrdup(props.get("kdenlive:clipname"));
                    info.sequenceId = qstrdup(props.get("id"));
                    info.sequenceDuration = qstrdup(props.get("kdenlive:duration"));
                    info.sequenceFrameDuration = m_masterProducer->time_to_frames(info.sequenceDuration.toUtf8().constData());
                    m_sequences.insert(sequenceUuid, info);
                }
            }
        }
        m_extraProperties.insert(i18n("Sequence count"), QString::number(m_sequences.size()));
        if (auto ptr = m_model.lock()) {
            QMetaObject::invokeMethod(ptr.get(), "loadSubSequences", Qt::QueuedConnection, Q_ARG(QString, m_binId), Q_ARG(sequenceMap, m_sequences));
        }
        QMapIterator<QUuid, SequenceInfo> ix2(m_sequences);
        while (ix2.hasNext()) {
            ix2.next();
            SequenceInfo info = ix2.value();
            m_extraProperties.insert(info.sequenceName, info.sequenceDuration);
        }
        generateTmpPlaylists();
    } else {
        qDebug() << "::::::::::\nRETAIN LIST INVALID\n\n::";
        // This is probably a library MLT clip, try reading xml root
        qDebug() << ":::: READING XMLROOT:" << s.get("kdenlive:projectroot");
        m_playlistRoot = QString(s.get("kdenlive:projectroot"));
    }
}

std::unique_ptr<Mlt::Producer> PlaylistClip::getThumbProducer(const QUuid &uuid)
{
    if (m_clipType == ClipType::Unknown || m_masterProducer == nullptr || m_clipStatus == FileStatus::StatusWaiting) {
        return nullptr;
    }
    if (!m_thumbMutex.tryLock()) {
        return nullptr;
    }
    std::unique_ptr<Mlt::Producer> thumbProd;
    QReadLocker lock(&pCore->xmlMutex);
    if (uuid.isNull()) {
        thumbProd.reset(masterProducer());
    } else {
        thumbProd = sequenceThumProducer(uuid);
    }
    m_thumbMutex.unlock();
    return thumbProd;
}

void PlaylistClip::createDisabledMasterProducer()
{
    if (!m_disabledProducer) {
        // Use dummy placeholder color clip
        m_disabledProducer = std::shared_ptr<Mlt::Producer>(new Mlt::Producer(pCore->getProjectProfile(), "color", "red"));
        Mlt::Properties original(m_masterProducer->get_properties());
        Mlt::Properties target(m_disabledProducer->get_properties());
        target.pass_list(original, "kdenlive:control_uuid,kdenlive:id,kdenlive:duration,kdenlive:maxduration,length");
        m_disabledProducer->set("set.test_audio", 1);
        m_disabledProducer->set("set.test_image", 1);
    }
}

const QString PlaylistClip::getSequenceResource()
{
    return QString();
}

const QString PlaylistClip::hashForThumbs()
{
    return ProjectClip::hashForThumbs();
}

const QString PlaylistClip::getFileHash()
{
    QPair<QByteArray, qint64> hashData = calculateHash(clipUrl());
    QByteArray fileHash = hashData.first;
    ClipController::setProducerProperty(QStringLiteral("kdenlive:file_size"), QString::number(hashData.second));
    if (fileHash.isEmpty()) {
        qDebug() << "// WARNING EMPTY CLIP HASH: ";
        return QString();
    }
    const QString result = fileHash.toHex();
    ClipController::setProducerProperty(QStringLiteral("kdenlive:file_hash"), result);
    return result;
}

void PlaylistClip::setProperties(const QMap<QString, QString> &properties, bool refreshPanel)
{
    ProjectClip::setProperties(properties, refreshPanel);
}

void PlaylistClip::removeSequenceWarpResources() {}

int PlaylistClip::getThumbFrame() const
{
    return qMax(0, getProducerIntProperty(QStringLiteral("kdenlive:thumbnailFrame")));
}

int PlaylistClip::getThumbFromPercent(int percent, bool storeFrame)
{
    return ProjectClip::getThumbFromPercent(percent, storeFrame);
}

bool PlaylistClip::canBeDropped(const QUuid &) const
{
    return true;
}

const QUuid PlaylistClip::getSequenceUuid() const
{
    return QUuid();
}

void PlaylistClip::setThumbFrame(int frame)
{
    ProjectClip::setThumbFrame(frame);
}

std::shared_ptr<Mlt::Producer> PlaylistClip::sequenceProducer(const QUuid &sequenceUuid)
{
    QReadLocker lock(&m_producerLock);
    if (sequenceUuid.isNull()) {
        return m_masterProducer;
    }
    // Parse producer's sequences to match
    Mlt::Service s(m_masterProducer->parent());
    Mlt::Properties retainList(mlt_properties(s.get_data("xml_retain")));
    if (retainList.is_valid()) {
        Mlt::Playlist playlist(mlt_playlist(retainList.get_data("main_bin")));
        int max = playlist.count();
        for (int i = 0; i < max; i++) {
            QScopedPointer<Mlt::Producer> prod(playlist.get_clip(i));
            if (prod->is_blank() || !prod->is_valid()) {
                continue;
            }
            if (prod->parent().type() == mlt_service_tractor_type) {
                const QUuid uuid(prod->parent().get("kdenlive:uuid"));
                if (uuid == sequenceUuid) {
                    return std::shared_ptr<Mlt::Producer>(new Mlt::Producer(prod->parent()));
                }
            }
        }
    }
    int maxDuration = m_masterProducer->parent().get_int("kdenlive:maxDuration");
    if (maxDuration > 0) {
        return std::make_shared<Mlt::Producer>(m_masterProducer->cut(0, maxDuration));
    }
    return m_masterProducer;
}

std::unique_ptr<Mlt::Producer> PlaylistClip::sequenceThumProducer(const QUuid &sequenceUuid)
{
    QReadLocker lock(&m_producerLock);
    // Parse producer's sequences to match
    Mlt::Service s(m_masterProducer->parent());
    Mlt::Properties retainList(mlt_properties(s.get_data("xml_retain")));
    if (retainList.is_valid()) {
        Mlt::Playlist playlist(mlt_playlist(retainList.get_data("main_bin")));
        int max = playlist.count();
        for (int i = 0; i < max; i++) {
            QScopedPointer<Mlt::Producer> prod(playlist.get_clip(i));
            if (prod->is_blank() || !prod->is_valid()) {
                continue;
            }
            if (prod->parent().type() == mlt_service_tractor_type) {
                const QUuid uuid(prod->parent().get("kdenlive:uuid"));
                if (uuid == sequenceUuid) {
                    return std::unique_ptr<Mlt::Producer>(new Mlt::Producer(prod->parent()));
                }
            }
        }
    }
    return nullptr;
}

size_t PlaylistClip::sequenceFrameDuration(const QUuid &uuid)
{
    if (!m_sequences.contains(uuid)) {
        return frameDuration();
    }
    return size_t(m_sequences.value(uuid).sequenceFrameDuration);
}

bool PlaylistClip::isActiveTimeline(const QUuid &uuid) const
{
    return (uuid != QUuid() && uuid == m_activeTimeline);
}

void PlaylistClip::generateTmpPlaylists()
{
    const QString resource = clipUrl();
    QFile sourceFile(resource);
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QDomDocument doc;
    if (!doc.setContent(&sourceFile)) {
        qWarning() << "Failed to parse file" << sourceFile.fileName() << "to QDomDocument";
        sourceFile.close();
        return;
    }
    sourceFile.close();
    // Find project tractor
    QDomNodeList tractors = doc.elementsByTagName(QStringLiteral("tractor"));
    QDomElement mainTrack;
    for (int i = 0; i < tractors.count(); ++i) {
        QDomElement trac = tractors.item(i).toElement();
        if (Xml::hasXmlProperty(trac, QStringLiteral("kdenlive:projectTractor"))) {
            mainTrack = trac.firstChildElement(QStringLiteral("track"));
            break;
        }
    }
    if (mainTrack.isNull()) {
        qWarning() << "Could not find main project tractor, aborting sub playlists creation";
        return;
    }
    QMapIterator<QUuid, SequenceInfo> ix(m_sequences);
    while (ix.hasNext()) {
        ix.next();
        if (ix.key() == m_activeTimeline) {
            //  Don't generate sequence for currently active sequence'
            continue;
        }
        SequenceInfo info = ix.value();
        QTemporaryFile tmp(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX.mlt")));
        tmp.setAutoRemove(false);
        if (tmp.open()) {
            tmp.close();
            QFile file(tmp.fileName());
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qDebug() << "Failed to generate temporary playlist file...";
            } else {
                // define main sequence
                mainTrack.setAttribute(QStringLiteral("producer"), info.sequenceId);
                mainTrack.setAttribute(QStringLiteral("out"), info.sequenceDuration);
                QTextStream outStream(&file);
                outStream << doc.toString();
                qDebug() << ":::: CREATED SEQUENCE " << info.sequenceName << " = " << tmp.fileName();
                m_sequencePlaylists.insert(ix.key(), tmp.fileName());
                file.close();
            }
        }
    }
}

std::shared_ptr<PlaylistSubClip> PlaylistClip::getSubSequence(const QUuid &uuid)
{
    for (int i = 0; i < childCount(); ++i) {
        std::shared_ptr<PlaylistSubClip> clip = std::static_pointer_cast<PlaylistSubClip>(child(i));
        if (clip && clip->sequenceUuid() == uuid) {
            return clip;
        }
    }
    return nullptr;
}

void PlaylistClip::setSequenceThumbnail(const QImage &img, const QUuid &uuid, bool /*inCache*/)
{
    if (img.isNull()) {
        return;
    }
    std::shared_ptr<PlaylistSubClip> sub = getSubSequence(uuid);
    if (sub) {
        sub->setThumbnail(img);
    }
}

const QString PlaylistClip::getPlaylistRoot()
{
    if (m_playlistRoot.isEmpty()) {
        return pCore->currentDoc()->documentRoot();
    }
    return m_playlistRoot;
}

bool PlaylistClip::hasAlpha()
{
    if (clipUrl().isEmpty()) {
        return false;
    }
    bool hasAlpha = false;
    QFile inputFile(clipUrl());
    if (inputFile.open(QIODevice::ReadOnly)) {
        const QString png = QStringLiteral(".png");
        const QString title = QStringLiteral(".kdenlivetitle");
        const QString kdenliveAlpha = QStringLiteral("kdenlive:has_alpha");
        const QString codecInfo = QStringLiteral(".codec.pix_fmt\">");
        QTextStream in(&inputFile);
        while (!in.atEnd()) {
            const QString line = in.readLine();
            if (line.contains(png) || line.contains(title) || line.contains(kdenliveAlpha)) {
                hasAlpha = true;
                break;
            }
            if (line.contains(codecInfo)) {
                if (line.contains(QStringLiteral("codec.pix_fmt\">argb")) || line.contains(QStringLiteral("codec.pix_fmt\">abgr")) ||
                    line.contains(QStringLiteral("codec.pix_fmt\">bgra")) || line.contains(QStringLiteral("codec.pix_fmt\">rgba")) ||
                    line.contains(QStringLiteral("codec.pix_fmt\">argb")) || line.contains(QStringLiteral("codec.pix_fmt\">yuva")) ||
                    line.contains(QStringLiteral("codec.pix_fmt\">ya"))) {
                    hasAlpha = true;
                    break;
                }
            }
        }
        inputFile.close();
    }
    return hasAlpha;
}
