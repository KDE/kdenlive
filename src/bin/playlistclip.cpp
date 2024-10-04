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
#include "jobs/audiolevelstask.h"
#include "jobs/cachetask.h"
#include "jobs/cliploadtask.h"
#include "jobs/proxytask.h"
#include "kdenlivesettings.h"
#include "lib/audio/audioStreamInfo.h"
#include "macros.hpp"
#include "mltcontroller/clippropertiescontroller.h"
#include "model/markerlistmodel.hpp"
#include "model/markersortmodel.h"
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
    QString tag = document.documentElement().tagName();
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
        int sequenceCount = 0;
        m_sequences.clear();
        for (int i = 0; i < max; i++) {
            QScopedPointer<Mlt::Producer> prod(playlist.get_clip(i));
            if (prod->is_blank() || !prod->is_valid()) {
                continue;
            }
            if (prod->parent().type() == mlt_service_tractor_type) {
                if (prod->parent().property_exists("kdenlive:uuid")) {
                    // Load sequence properties
                    sequenceCount++;
                    SequenceInfo info;
                    Mlt::Properties props(prod->parent());
                    const QUuid controlUuid(props.get("kdenlive:control_uuid"));
                    info.sequenceName = qstrdup(props.get("kdenlive:clipname"));
                    info.sequenceId = qstrdup(props.get("id"));
                    info.sequenceDuration = qstrdup(props.get("kdenlive:duration"));
                    info.sequenceFrameDuration = m_masterProducer->time_to_frames(info.sequenceDuration.toUtf8().constData());
                    m_sequences.insert(controlUuid, info);
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
    } else {
        qDebug() << "::::::::::\nRETAIN LIST INVALID\n\n::";
    }
}

std::unique_ptr<Mlt::Producer> PlaylistClip::getThumbProducer()
{
    if (m_clipType == ClipType::Unknown || m_masterProducer == nullptr || m_clipStatus == FileStatus::StatusWaiting) {
        return nullptr;
    }
    if (!m_thumbMutex.tryLock()) {
        return nullptr;
    }
    std::unique_ptr<Mlt::Producer> thumbProd;
    QReadLocker lock(&pCore->xmlMutex);
    thumbProd.reset(masterProducer());
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
                const QUuid uuid(prod->parent().get("kdenlive:control_uuid"));
                if (uuid == sequenceUuid) {
                    return std::shared_ptr<Mlt::Producer>(new Mlt::Producer(prod->parent()));
                }
            }
        }
    }
    return m_masterProducer;
}

size_t PlaylistClip::sequenceFrameDuration(const QUuid &uuid)
{
    if (!m_sequences.contains(uuid)) {
        return frameDuration();
    }
    return size_t(m_sequences.value(uuid).sequenceFrameDuration);
}
