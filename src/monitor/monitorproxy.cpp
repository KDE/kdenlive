/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "monitorproxy.h"
#include "bin/bin.h"
#include "core.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "monitormanager.h"
#include "profiles/profilemodel.hpp"

#include <QUuid>

#include <mlt++/MltConsumer.h>
#include <mlt++/MltFilter.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

MonitorProxy::MonitorProxy(VideoWidget *parent)
    : QObject(parent)
    , q(parent)
    , m_position(-1)
    , m_zoneIn(0)
    , m_zoneOut(-1)
    , m_hasAV(false)
    , m_speed(0)
    , m_clipType(0)
    , m_clipId(-1)
    , m_seekFinished(true)
    , m_td(nullptr)
    , m_trimmingFrames1(0)
    , m_trimmingFrames2(0)
    , m_boundsCount(0)
{
    if (q->m_id == int(Kdenlive::ClipMonitor)) {
        connect(pCore->bin(), &Bin::clipNameChanged, this, &MonitorProxy::updateClipName);
    }
    m_showGrid = KdenliveSettings::showMonitorGrid();
}

void MonitorProxy::switchGrid()
{
    m_showGrid = !m_showGrid;
    KdenliveSettings::setShowMonitorGrid(m_showGrid);
    Q_EMIT showGridChanged();
}

int MonitorProxy::gridH() const
{
    return KdenliveSettings::monitorGridH();
}

int MonitorProxy::gridV() const
{
    return KdenliveSettings::monitorGridV();
}

int MonitorProxy::getPosition() const
{
    return m_position;
}

void MonitorProxy::updateClipName(int id, const QString newName)
{
    for (int i = 0; i < m_lastClipsIds.size(); i++) {
        if (m_lastClipsIds.at(i).first == id) {
            m_lastClipsIds.remove(i);
            m_lastClipsIds.insert(i, {id, newName});
            m_lastClips.clear();
            for (int j = 0; j < 4 && j < m_lastClipsIds.size(); j++) {
                m_lastClips << m_lastClipsIds.at(j).second;
            }
            m_clipName = newName;
            Q_EMIT clipNameChanged();
            Q_EMIT lastClipsChanged();
            break;
        }
    }
}

void MonitorProxy::resetPosition()
{
    m_position = -1;
}

int MonitorProxy::rulerHeight() const
{
    return q->m_rulerHeight;
}

void MonitorProxy::setRulerHeight(int addedHeight)
{
    q->updateRulerHeight(addedHeight);
}

void MonitorProxy::seek(int delta, uint modifiers)
{
    Q_EMIT q->mouseSeek(delta, modifiers);
}

int MonitorProxy::overlayType() const
{
    return (q->m_id == int(Kdenlive::ClipMonitor) ? KdenliveSettings::clipMonitorOverlayGuides() : KdenliveSettings::projectMonitorOverlayGuides());
}

void MonitorProxy::setOverlayType(int ix)
{
    if (q->m_id == int(Kdenlive::ClipMonitor)) {
        KdenliveSettings::setClipMonitorOverlayGuides(ix);
    } else {
        KdenliveSettings::setProjectMonitorOverlayGuides(ix);
    }
}

bool MonitorProxy::setPosition(int pos)
{
    return setPositionAdvanced(pos, false);
}

bool MonitorProxy::setPositionAdvanced(int pos, bool noAudioScrub)
{
    if (m_position == pos) {
        return true;
    }
    m_position = pos;
    Q_EMIT requestSeek(pos, noAudioScrub);
    if (m_seekFinished) {
        m_seekFinished = false;
        Q_EMIT seekFinishedChanged();
    }
    Q_EMIT positionChanged(pos);
    return false;
}

void MonitorProxy::setCursorPosition(int pos)
{
    m_position = pos;
    Q_EMIT positionChanged(pos);
}

void MonitorProxy::positionFromConsumer(int pos, bool playing)
{
    if (playing) {
        m_position = pos;
        Q_EMIT positionChanged(pos);
        if (!m_seekFinished) {
            m_seekFinished = true;
            Q_EMIT seekFinishedChanged();
        }
    } else {
        if (!m_seekFinished && m_position == pos) {
            m_seekFinished = true;
            Q_EMIT seekFinishedChanged();
        }
    }
}

void MonitorProxy::setMarker(const QString &comment, const QColor &color)
{
    if (m_markerComment == comment && color == m_markerColor) {
        return;
    }
    m_markerComment = comment;
    m_markerColor = color;
    Q_EMIT markerChanged();
}

int MonitorProxy::zoneIn() const
{
    return m_zoneIn;
}

int MonitorProxy::zoneOut() const
{
    return m_zoneOut;
}

void MonitorProxy::setZoneIn(int pos)
{
    if (m_zoneIn > 0) {
        Q_EMIT removeSnap(m_zoneIn);
    }
    m_zoneIn = pos;
    if (pos > 0) {
        Q_EMIT addSnap(pos);
    }
    Q_EMIT zoneChanged();
    Q_EMIT saveZone(QPoint(m_zoneIn, m_zoneOut));
}

void MonitorProxy::setZoneOut(int pos)
{
    if (m_zoneOut > 0) {
        Q_EMIT removeSnap(m_zoneOut);
    }
    m_zoneOut = pos;
    if (pos > 0) {
        Q_EMIT addSnap(m_zoneOut);
    }
    Q_EMIT zoneChanged();
    Q_EMIT saveZone(QPoint(m_zoneIn, m_zoneOut));
}

void MonitorProxy::startZoneMove()
{
    m_undoZone = QPoint(m_zoneIn, m_zoneOut);
}

void MonitorProxy::endZoneMove()
{
    Q_EMIT saveZoneWithUndo(m_undoZone, QPoint(m_zoneIn, m_zoneOut));
}

void MonitorProxy::setZone(int in, int out, bool sendUpdate)
{
    if (m_zoneIn > 0) {
        Q_EMIT removeSnap(m_zoneIn);
    }
    if (m_zoneOut > 0) {
        Q_EMIT removeSnap(m_zoneOut);
    }
    m_zoneIn = in;
    m_zoneOut = out;
    if (m_zoneIn > 0) {
        Q_EMIT addSnap(m_zoneIn);
    }
    if (m_zoneOut > 0) {
        Q_EMIT addSnap(m_zoneOut);
    }
    Q_EMIT zoneChanged();
    if (sendUpdate) {
        Q_EMIT saveZone(QPoint(m_zoneIn, m_zoneOut));
    }
}

void MonitorProxy::setZone(QPoint zone, bool sendUpdate)
{
    setZone(zone.x(), zone.y(), sendUpdate);
}

void MonitorProxy::resetZone()
{
    m_zoneIn = 0;
    m_zoneOut = -1;
    m_clipBounds = {};
    m_boundsCount = 0;
    Q_EMIT clipBoundsChanged();
    Q_EMIT zoneChanged();
}

double MonitorProxy::fps() const
{
    return pCore->getCurrentFps();
}

QPoint MonitorProxy::zone() const
{
    return {m_zoneIn, m_zoneOut};
}

void MonitorProxy::extractFrameToFile(int frame_position, const QStringList &pathInfo, bool addToProject, bool useSourceProfile)
{
    const QString path = pathInfo.at(0);
    const QString destPath = pathInfo.at(1);
    const QString folderInfo = pathInfo.at(2);
    QSize finalSize = pCore->getCurrentFrameDisplaySize();
    QSize size = pCore->getCurrentFrameSize();
    int height = size.height();
    int width = size.width();
    if (path.isEmpty()) {
        // Use current monitor producer to extract frame
        Mlt::Frame *frame = q->m_producer->get_frame();
        QImage img = KThumb::getFrame(frame, width, height, finalSize.width());
        delete frame;
        img.save(destPath);
        if (addToProject) {
            QMetaObject::invokeMethod(pCore->bin(), "droppedUrls", Q_ARG(const QList<QUrl> &, {QUrl::fromLocalFile(destPath)}),
                                      Q_ARG(const QString &, folderInfo));
        }
        return;
    }
    QScopedPointer<Mlt::Producer> producer;
    QScopedPointer<Mlt::Profile> tmpProfile;
    if (useSourceProfile) {
        tmpProfile.reset(new Mlt::Profile());
        producer.reset(new Mlt::Producer(*tmpProfile, path.toUtf8().constData()));
    } else {
        producer.reset(new Mlt::Producer(pCore->getProjectProfile(), path.toUtf8().constData()));
    }
    if (producer && producer->is_valid()) {
        if (useSourceProfile) {
            tmpProfile->from_producer(*producer);
            width = tmpProfile->width();
            height = tmpProfile->height();
            if (tmpProfile->sar() != 1.) {
                finalSize.setWidth(qRound(height * tmpProfile->dar()));
            } else {
                finalSize.setWidth(0);
            }
            double projectFps = pCore->getCurrentFps();
            double currentFps = tmpProfile->fps();
            if (!qFuzzyCompare(projectFps, currentFps)) {
                frame_position = int(frame_position * currentFps / projectFps);
            }
        }
        QImage img = KThumb::getFrame(producer.data(), frame_position, width, height, finalSize.width());
        img.save(destPath);
        if (addToProject) {
            QMetaObject::invokeMethod(pCore->bin(), "droppedUrls", Q_ARG(const QList<QUrl> &, {QUrl::fromLocalFile(destPath)}),
                                      Q_ARG(const QString &, folderInfo));
        }
    } else {
        qDebug() << "::: INVALID PRODUCER: " << path;
    }
    if (QDir::temp().exists(path)) {
        // This was a temporary playlist file, remove
        QFile::remove(path);
    }
}

QImage MonitorProxy::extractFrame(const QString &path, int width, int height, bool useSourceProfile)
{
    if (width == -1) {
        width = pCore->getCurrentProfile()->width();
        height = pCore->getCurrentProfile()->height();
    } else if (width % 2 == 1) {
        width++;
    }

    if ((q->m_producer == nullptr) || !path.isEmpty()) {
        QImage pix(width, height, QImage::Format_RGB32);
        pix.fill(Qt::black);
        return pix;
    }
    Mlt::Frame *frame = nullptr;
    QImage img;
    if (useSourceProfile) {
        // Our source clip's resolution is higher than current profile, export at full res
        QScopedPointer<Mlt::Profile> tmpProfile(new Mlt::Profile());
        QString service = q->m_producer->get("mlt_service");
        QScopedPointer<Mlt::Producer> tmpProd(new Mlt::Producer(*tmpProfile, service.toUtf8().constData(), q->m_producer->get("resource")));
        tmpProfile->from_producer(*tmpProd);
        width = tmpProfile->width();
        height = tmpProfile->height();
        if (tmpProd && tmpProd->is_valid()) {
            Mlt::Filter scaler(*tmpProfile, "swscale");
            Mlt::Filter converter(*tmpProfile, "avcolor_space");
            tmpProd->attach(scaler);
            tmpProd->attach(converter);
            // TODO: paste effects
            // Clip(*tmpProd).addEffects(*q->m_producer);
            double projectFps = pCore->getCurrentFps();
            double currentFps = tmpProfile->fps();
            if (qFuzzyCompare(projectFps, currentFps)) {
                tmpProd->seek(q->m_producer->position());
            } else {
                int maxLength = int(q->m_producer->get_length() * currentFps / projectFps);
                tmpProd->set("length", maxLength);
                tmpProd->set("out", maxLength - 1);
                tmpProd->seek(int(q->m_producer->position() * currentFps / projectFps));
            }
            frame = tmpProd->get_frame();
            img = KThumb::getFrame(frame, width, height);
            delete frame;
        }
    } else if (KdenliveSettings::gpu_accel()) {
        QString service = q->m_producer->get("mlt_service");
        QScopedPointer<Mlt::Producer> tmpProd(new Mlt::Producer(pCore->getProjectProfile(), service.toUtf8().constData(), q->m_producer->get("resource")));
        Mlt::Filter scaler(pCore->getProjectProfile(), "swscale");
        Mlt::Filter converter(pCore->getProjectProfile(), "avcolor_space");
        tmpProd->attach(scaler);
        tmpProd->attach(converter);
        tmpProd->seek(q->m_producer->position());
        frame = tmpProd->get_frame();
        img = KThumb::getFrame(frame, width, height);
        delete frame;
    } else {
        frame = q->m_producer->get_frame();
        img = KThumb::getFrame(frame, width, height);
        delete frame;
    }
    return img;
}

void MonitorProxy::activateClipMonitor(bool isClipMonitor)
{
    pCore->monitorManager()->activateMonitor(isClipMonitor ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor);
}

QString MonitorProxy::toTimecode(int frames) const
{
    return KdenliveSettings::frametimecode() ? QString::number(frames) : q->frameToTime(frames);
}

void MonitorProxy::selectClip(int ix)
{
    if (ix < m_lastClipsIds.size()) {
        int cid = m_lastClipsIds.at(ix).first;
        pCore->bin()->selectClipById(QString::number(cid));
    }
}

void MonitorProxy::setClipProperties(int clipId, ClipType::ProducerType type, bool hasAV, const QString &clipName)
{
    if (clipId != m_clipId) {
        m_clipId = clipId;
        Q_EMIT clipIdChanged();
    }
    if (hasAV != m_hasAV) {
        m_hasAV = hasAV;
        Q_EMIT clipHasAVChanged();
    }
    if (type != m_clipType) {
        m_clipType = type;
        Q_EMIT clipTypeChanged();
    }
    if (!clipName.isEmpty()) {
        std::pair<int, QString> id = {clipId, clipName};
        for (int i = 0; i < m_lastClipsIds.size(); i++) {
            if (m_lastClipsIds.at(i).first == clipId) {
                m_lastClipsIds.remove(i);
                break;
            }
        }
        m_lastClipsIds.prepend(id);
        while (m_lastClipsIds.size() > 4) {
            m_lastClipsIds.removeLast();
        }
        m_lastClips.clear();
        for (int i = 0; i < 4 && i < m_lastClipsIds.size(); i++) {
            m_lastClips << m_lastClipsIds.at(i).second;
        }
        Q_EMIT lastClipsChanged();
    }
    if (clipName == m_clipName) {
        m_clipName.clear();
        Q_EMIT clipNameChanged();
    }
    m_clipName = clipName;
    Q_EMIT clipNameChanged();
}

void MonitorProxy::clipDeleted(int cid)
{
    for (int i = 0; i < m_lastClipsIds.size(); i++) {
        if (m_lastClipsIds.at(i).first == cid) {
            m_lastClipsIds.remove(i);
            m_lastClips.clear();
            for (int j = 0; j < 4 && j < m_lastClipsIds.size(); j++) {
                m_lastClips << m_lastClipsIds.at(j).second;
            }
            Q_EMIT lastClipsChanged();
            break;
        }
    }
}

void MonitorProxy::documentClosed()
{
    m_lastClipsIds.clear();
    m_lastClips.clear();
    m_clipName.clear();
    Q_EMIT lastClipsChanged();
    Q_EMIT clipNameChanged();
}

void MonitorProxy::setAudioThumb(const QList<int> &streamIndexes, const QList<int> &channels)
{
    m_audioChannels = channels;
    m_audioStreams = streamIndexes;
    Q_EMIT audioThumbChanged();
}

void MonitorProxy::setAudioStream(const QString &name)
{
    m_clipStream = name;
    Q_EMIT clipStreamChanged();
}

QPoint MonitorProxy::profile()
{
    QSize s = pCore->getCurrentFrameSize();
    return QPoint(s.width(), s.height());
}

QColor MonitorProxy::thumbColor1() const
{
    return KdenliveSettings::thumbColor1();
}

QColor MonitorProxy::thumbColor2() const
{
    return KdenliveSettings::thumbColor2();
}

QColor MonitorProxy::overlayColor() const
{
    return KdenliveSettings::overlayColor();
}

bool MonitorProxy::audioThumbFormat() const
{
    return KdenliveSettings::displayallchannels();
}

bool MonitorProxy::audioThumbNormalize() const
{
    return KdenliveSettings::normalizechannels();
}

void MonitorProxy::switchAutoKeyframe()
{
    KdenliveSettings::setAutoKeyframe(!KdenliveSettings::autoKeyframe());
    Q_EMIT autoKeyframeChanged();
}

bool MonitorProxy::autoKeyframe() const
{
    return KdenliveSettings::autoKeyframe();
}

const QString MonitorProxy::timecode() const
{
    if (m_td) {
        return m_td->displayText();
    }
    return QString();
}

const QString MonitorProxy::trimmingTC1() const
{
    return toTimecode(m_trimmingFrames1);
}

const QString MonitorProxy::trimmingTC2() const
{
    return toTimecode(m_trimmingFrames2);
}

void MonitorProxy::setTimeCode(TimecodeDisplay *td)
{
    m_td = td;
    connect(m_td, &TimecodeDisplay::timeCodeUpdated, this, &MonitorProxy::timecodeChanged);
}

void MonitorProxy::setTrimmingTC1(int frames, bool isRelativ)
{
    if (isRelativ) {
        m_trimmingFrames1 -= frames;
    } else {
        m_trimmingFrames1 = frames;
    }
    Q_EMIT trimmingTC1Changed();
}

void MonitorProxy::setTrimmingTC2(int frames, bool isRelativ)
{
    if (isRelativ) {
        m_trimmingFrames2 -= frames;
    } else {
        m_trimmingFrames2 = frames;
    }
    Q_EMIT trimmingTC2Changed();
}

void MonitorProxy::setWidgetKeyBinding(const QString &text) const
{
    pCore->setWidgetKeyBinding(text);
}

void MonitorProxy::setSpeed(double speed)
{
    if (qAbs(m_speed) > 1. || qAbs(speed) > 1.) {
        // check if we have or had a speed > 1 or < -1
        m_speed = speed;
        Q_EMIT speedChanged();
    }
}

QByteArray MonitorProxy::getUuid() const
{
    return QUuid::createUuid().toByteArray();
}

void MonitorProxy::updateClipBounds(const QVector<QPoint> &bounds)
{
    if (bounds.size() == m_boundsCount) {
        // Enforce refresh, in/out points may have changed
        m_boundsCount = 0;
        Q_EMIT clipBoundsChanged();
    }
    m_clipBounds = bounds;
    m_boundsCount = bounds.size();
    Q_EMIT clipBoundsChanged();
}

const QPoint MonitorProxy::clipBoundary(int ix)
{
    return m_clipBounds.at(ix);
}

bool MonitorProxy::seekOnDrop() const
{
    return KdenliveSettings::seekonaddeffect();
}

void MonitorProxy::addEffect(const QString &effectData, const QString &effectSource)
{
    QStringList effectInfo = effectSource.split(QLatin1Char(','));
    effectInfo.prepend(effectData);
    if (m_clipId > -1) {
        QMetaObject::invokeMethod(pCore->bin(), "slotAddEffect", Qt::QueuedConnection, Q_ARG(std::vector<QString>, {QString::number(m_clipId)}),
                                  Q_ARG(QStringList, effectInfo));
    } else {
        // Dropped in project monitor
        QMetaObject::invokeMethod(this, "addTimelineEffect", Qt::QueuedConnection, Q_ARG(QStringList, effectInfo));
    }
}

void MonitorProxy::setJobsProgress(const ObjectId &owner, const QStringList &jobNames, const QList<int> &jobProgress, const QStringList &jobUuids)
{
    if (owner.itemId != m_clipId) {
        // Not interested
        return;
    }

    m_jobsProgress = jobProgress;
    m_jobsUuids = jobUuids;
    if (m_runningJobs != jobNames) {
        m_runningJobs = jobNames;
        Q_EMIT runningJobsChanged();
    }
    Q_EMIT jobsProgressChanged();
}

void MonitorProxy::clearJobsProgress()
{
    if (!m_runningJobs.isEmpty()) {
        m_jobsProgress.clear();
        m_jobsUuids.clear();
        m_runningJobs.clear();
        Q_EMIT runningJobsChanged();
    }
}

void MonitorProxy::terminateJob(const QString &uuid)
{
    pCore->taskManager.discardJob(ObjectId(KdenliveObjectType::BinClip, m_clipId, QUuid()), QUuid(uuid));
}
