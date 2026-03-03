/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "sequenceclip.h"
#include "bin.h"
#include "clipcreator.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "jobs/audiolevels/audiolevelstask.h"
#include "jobs/cachetask.h"
#include "jobs/cliploadtask.h"
#include "kdenlivesettings.h"
#include "mltcontroller/clippropertiescontroller.h"
#include "model/markerlistmodel.hpp"
#include "monitor/monitor.h"
#include "project/projectmanager.h"
#include "projectfolder.h"
#include "projectitemmodel.h"
#include "projectsubclip.h"
#include "timeline2/model/timelineitemmodel.hpp"
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
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include <rttr/registration>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
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
    connect(this, &ProjectClip::audioThumbReady, this, &SequenceClip::updateAudioSync);
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
    connect(this, &ProjectClip::audioThumbReady, this, &SequenceClip::updateAudioSync);
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
    QString tag = document.documentElement().tagName();
    QDomElement prod = document.documentElement();
    if (tag != QLatin1String("producer") && tag != QLatin1String("chain")) {
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
    if (m_masterProducer) {
        // Pass sequence properties
        producer->parent().pass_list(m_masterProducer->parent(), "kdenlive:clipzones,kdenlive:zone_in,kdenlive:zone_out");
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

QTemporaryFile *SequenceClip::getSequenceTmpResource()
{
    QTemporaryFile *tmp = new QTemporaryFile(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX.mlt")));
    if (tmp->open()) {
        tmp->close();
        cloneProducerToFile(tmp->fileName(), false, true);
        return tmp;
    }
    return nullptr;
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
    for (auto i = properties.cbegin(), end = properties.cend(); i != end; ++i) {
        if (i.key().startsWith(QLatin1String("kdenlive:sequenceproperties."))) {
            pCore->currentDoc()->setSequenceProperty(m_sequenceUuid, i.key(), i.value());
            if (i.key() == QLatin1String("kdenlive:sequenceproperties.timecodeOffset")) {
                pCore->projectManager()->updateSequenceOffset(m_sequenceUuid);
            }
        }
    }
    bool durationChanged = properties.contains("length") && properties.value("length").toInt() != getFramePlaytime();
    qDebug() << ":::: SEQUENCE DURATION CHANGED: " << properties.value("length").toInt() << " != " << getFramePlaytime();
    ProjectClip::setProperties(properties, refreshPanel);
    if (properties.contains(QStringLiteral("kdenlive:clipname"))) {
        if (!m_sequenceUuid.isNull()) {
            // This is a timeline clip, update tab name
            Q_EMIT pCore->bin()->updateTabName(m_sequenceUuid, m_name);
        }
    }
    if (durationChanged) {
        discardAudioThumb();
        m_audioSynced = false;
        if (pCore->taskManager.displayedClip == m_binId.toInt()) {
            // Mark audio thumb as dirty
            pCore->getMonitor(Kdenlive::ClipMonitor)->markAudioDirty(true);
            // Refresh monitor duration
            pCore->getMonitor(Kdenlive::ClipMonitor)->adjustRulerSize(properties.value("length").toInt());
        }
    }
}

void SequenceClip::markAudioDirty()
{
    if (!m_audioSynced) {
        // Already dirty, abort
        return;
    }
    discardAudioThumb();
    m_audioSynced = false;
    if (pCore->taskManager.displayedClip == m_binId.toInt()) {
        // Mark audio thumb as dirty
        pCore->getMonitor(Kdenlive::ClipMonitor)->markAudioDirty(true);
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

void SequenceClip::saveAudioWave()
{
    const QString cachePath = getAudioThumbPath(0);
    if (!m_audioSynced) {
        // Delete existing audio cache if any
        if (!cachePath.isEmpty()) {
            QFile::remove(cachePath);
        }
        return;
    }
    QVector<int16_t> levels = audioFrameCache(0);
    if (levels.isEmpty()) {
        qDebug() << ":: AUDIO LEVELS EMPTY FOR CURRENT TIMELINE...";
        return;
    }
    QFile file(cachePath);
    if (file.open(QIODevice::WriteOnly)) {
        QDataStream out(&file);
        out << levels;
        file.close();
    } else {
        qWarning() << "Could not write to audiothumb file: " << cachePath;
    }
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

int SequenceClip::lastBound()
{
    if (m_registeredClipsByUuid.isEmpty()) {
        return pCore->currentDoc()->getSequenceProperty(m_sequenceUuid, QStringLiteral("lastUsedFrame")).toInt();
    }
    QMapIterator<QUuid, QList<int>> i(m_registeredClipsByUuid);
    int lastUsedFrame = 0;
    while (i.hasNext()) {
        i.next();
        QList<int> instances = i.value();
        if (!instances.isEmpty()) {
            auto timeline = pCore->currentDoc()->getTimeline(i.key());
            if (!timeline) {
                qDebug() << "Error while reloading clip: timeline unavailable";
                Q_ASSERT(false);
            }
            for (auto &cid : instances) {
                QPoint p = timeline->getClipInDuration(cid);
                lastUsedFrame = qMax(lastUsedFrame, p.x() + p.y());
            }
        }
    }
    return lastUsedFrame;
}

std::shared_ptr<Mlt::Producer> SequenceClip::sequenceProducer(const QUuid &)
{
    QReadLocker lock(&m_producerLock);
    int maxDuration = m_masterProducer->parent().get_int("kdenlive:maxduration");
    if (maxDuration > 0) {
        return std::make_shared<Mlt::Producer>(m_masterProducer->cut(0, maxDuration - 1));
    }
    return m_masterProducer;
}

int SequenceClip::getStartTimecode()
{
    return pCore->currentDoc()->getSequenceProperty(m_sequenceUuid, "kdenlive:sequenceproperties.timecodeOffset").toInt();
}

const QString SequenceClip::hash(bool /*createIfEmpty*/)
{
    return m_sequenceUuid.toString();
}

bool SequenceClip::audioSynced() const
{
    return m_audioSynced;
}

void SequenceClip::updateAudioSync()
{
    m_audioSynced = true;
    if (pCore->taskManager.displayedClip == m_binId.toInt()) {
        // Mark audio thumb as dirty
        pCore->getMonitor(Kdenlive::ClipMonitor)->markAudioDirty(false);
    }
}
