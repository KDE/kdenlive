/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "sequenceclip.h"
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
    registration::class_<SequenceClip>("SequenceClip");
}
#endif

SequenceClip::SequenceClip(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model, std::shared_ptr<Mlt::Producer> &producer)
    : ProjectClip(id, thumb, model, producer)
{
    // Initialize path for thumbnails playlist
    m_sequenceUuid = QUuid(m_masterProducer->get("kdenlive:uuid"));
    m_clipType = ClipType::Timeline;
    if (model->hasSequenceId(m_sequenceUuid)) {
        // We already have a sequence with this uuid, this is probably a duplicate, update uuid
        const QUuid prevUuid = m_sequenceUuid;
        m_sequenceUuid = QUuid::createUuid();
        m_masterProducer->set("kdenlive:uuid", m_sequenceUuid.toString().toUtf8().constData());
        m_masterProducer->parent().set("kdenlive:uuid", m_sequenceUuid.toString().toUtf8().constData());
        const QString subValue(m_masterProducer->get("kdenlive:sequenceproperties.subtitlesList"));
        if (!subValue.isEmpty()) {
            int ix = m_masterProducer->get_int("kdenlive:sequenceproperties.kdenlive:activeSubtitleIndex");
            pCore->currentDoc()->setSequenceProperty(m_sequenceUuid, QStringLiteral("kdenlive:activeSubtitleIndex"), QString::number(ix));
            pCore->currentDoc()->duplicateSequenceProperty(m_sequenceUuid, prevUuid, subValue);
        }
    }
    m_sequenceThumbFile.setFileTemplate(QDir::temp().absoluteFilePath(QStringLiteral("thumbs-%1-XXXXXX.mlt").arg(m_binId)));
    // Timeline clip thumbs will be generated later after the tractor has been updated
    qDebug() << "555555555555555555\n\nBUILDING SEQUENCE CLIP\n\n555555555555555555555555555";
}

// static
std::shared_ptr<SequenceClip> SequenceClip::construct(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model,
                                                      std::shared_ptr<Mlt::Producer> &producer)
{
    std::shared_ptr<SequenceClip> self(new SequenceClip(id, thumb, model, producer));
    baseFinishConstruct(self);
    QMetaObject::invokeMethod(model.get(), "loadSubClips", Qt::QueuedConnection, Q_ARG(QString, id),
                              Q_ARG(QString, self->getProducerProperty(QStringLiteral("kdenlive:clipzones"))), Q_ARG(bool, false));
    return self;
}

SequenceClip::SequenceClip(const QString &id, const QDomElement &description, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model)
    : ProjectClip(id, description, thumb, model)
{
    m_sequenceUuid = QUuid(getXmlProperty(description, QStringLiteral("kdenlive:uuid")));
}

std::shared_ptr<SequenceClip> SequenceClip::construct(const QString &id, const QDomElement &description, const QIcon &thumb,
                                                      std::shared_ptr<ProjectItemModel> model)
{
    std::shared_ptr<SequenceClip> self(new SequenceClip(id, description, thumb, std::move(model)));
    baseFinishConstruct(self);
    return self;
}

SequenceClip::~SequenceClip() {}

QDomElement SequenceClip::toXml(QDomDocument &document, bool includeMeta, bool includeProfile)
{
    getProducerXML(document, includeMeta, includeProfile);
    QDomElement prod;
    QString tag = document.documentElement().tagName();
    if (tag == QLatin1String("producer") || tag == QLatin1String("chain")) {
        prod = document.documentElement();
    } else {
        // This is a sequence clip
        prod = document.documentElement();
        prod.setAttribute(QStringLiteral("kdenlive:id"), m_binId);
        prod.setAttribute(QStringLiteral("kdenlive:producer_type"), ClipType::Timeline);
        prod.setAttribute(QStringLiteral("kdenlive:uuid"), m_sequenceUuid.toString());
        prod.setAttribute(QStringLiteral("kdenlive:duration"), QString::number(frameDuration()));
        prod.setAttribute(QStringLiteral("kdenlive:clipname"), clipName());
    }
    prod.setAttribute(QStringLiteral("type"), int(m_clipType));
    return prod;
}

bool SequenceClip::setProducer(std::shared_ptr<Mlt::Producer> producer, bool generateThumb, bool clearTrackProducers)
{
    if (m_sequenceUuid.isNull()) {
        m_sequenceUuid = QUuid::createUuid();
        producer->parent().set("kdenlive:uuid", m_sequenceUuid.toString().toUtf8().constData());
    }
    if (m_timewarpProducers.size() > 0) {
        bool ok;
        QDir sequenceFolder = pCore->currentDoc()->getCacheDir(CacheTmpWorkFiles, &ok);
        if (ok) {
            QString resource = sequenceFolder.absoluteFilePath(QString("sequence-%1.mlt").arg(m_sequenceUuid.toString()));
            QFile::remove(resource);
        }
    }
    return ProjectClip::setProducer(producer, generateThumb, clearTrackProducers);
}

std::unique_ptr<Mlt::Producer> SequenceClip::getThumbProducer(const QUuid &)
{
    if (m_clipType == ClipType::Unknown || m_masterProducer == nullptr || m_clipStatus == FileStatus::StatusWaiting ||
        m_clipStatus == FileStatus::StatusMissing) {
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

void SequenceClip::createDisabledMasterProducer()
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

const QString SequenceClip::getSequenceResource()
{
    // speed effects of sequence clips have to use an external mlt playslist file
    bool ok;
    QDir sequenceFolder = pCore->currentDoc()->getCacheDir(CacheTmpWorkFiles, &ok);
    if (!ok) {
        qWarning() << "Cannot write to cache folder: " << sequenceFolder.absolutePath();
        return nullptr;
    }
    QString resource = sequenceFolder.absoluteFilePath(QString("sequence-%1.mlt").arg(m_sequenceUuid.toString()));
    if (!QFileInfo::exists(resource)) {
        cloneProducerToFile(resource);
    }
    return resource;
}

const QString SequenceClip::hashForThumbs()
{
    if (m_clipStatus == FileStatus::StatusWaiting) {
        // Clip is not ready
        return QString();
    }
    return m_sequenceUuid.toString();
}

const QString SequenceClip::getFileHash()
{
    const QByteArray fileData = m_sequenceUuid.toString().toUtf8();
    const QByteArray fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
    if (fileHash.isEmpty()) {
        qDebug() << "// WARNING EMPTY CLIP HASH: ";
        return QString();
    }
    QString result = fileHash.toHex();
    ClipController::setProducerProperty(QStringLiteral("kdenlive:file_hash"), result);
    return result;
}

void SequenceClip::setProperties(const QMap<QString, QString> &properties, bool refreshPanel)
{
    ProjectClip::setProperties(properties, refreshPanel);
    if (properties.contains(QStringLiteral("kdenlive:clipname"))) {
        const QString updatedName = properties.value(QStringLiteral("kdenlive:clipname"));
        if (!m_sequenceUuid.isNull()) {
            // This is a timeline clip, update tab name
            Q_EMIT pCore->bin()->updateTabName(m_sequenceUuid, m_name);
        }
    }
}

void SequenceClip::removeSequenceWarpResources()
{
    if (m_timewarpProducers.size() > 0 && pCore->window() && pCore->bin()->isEnabled()) {
        bool ok;
        QDir sequenceFolder = pCore->currentDoc()->getCacheDir(CacheTmpWorkFiles, &ok);
        if (ok) {
            QString resource = sequenceFolder.absoluteFilePath(QString("sequence-%1.mlt").arg(m_sequenceUuid.toString()));
            QFile::remove(resource);
        }
    }
}

int SequenceClip::getThumbFrame() const
{
    return qMax(0, pCore->currentDoc()->getSequenceProperty(m_sequenceUuid, QStringLiteral("thumbnailFrame")).toInt());
}

int SequenceClip::getThumbFromPercent(int percent, bool storeFrame)
{
    int framePos = ProjectClip::getThumbFromPercent(percent, storeFrame);
    pCore->currentDoc()->setSequenceProperty(m_sequenceUuid, QStringLiteral("thumbnailFrame"), framePos);
    return framePos;
}

bool SequenceClip::canBeDropped(const QUuid &uuid) const
{
    if (m_sequenceUuid == uuid) {
        return false;
    }
    if (auto ptr = m_model.lock()) {
        return std::static_pointer_cast<ProjectItemModel>(ptr)->canBeEmbeded(uuid, m_sequenceUuid);
    } else {
        qDebug() << "..... ERROR CANNOT LOCK MODEL";
    }
    return true;
}

const QUuid SequenceClip::getSequenceUuid() const
{
    return m_sequenceUuid;
}

void SequenceClip::setThumbFrame(int frame)
{
    resetSequenceThumbnails();
    pCore->currentDoc()->setSequenceProperty(m_sequenceUuid, QStringLiteral("thumbnailFrame"), frame);
    ClipLoadTask::start(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), QDomElement(), true, -1, -1, this);
}
