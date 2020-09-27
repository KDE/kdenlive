/***************************************************************************
 *   Copyright (C) 2018 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "monitorproxy.h"
#include "core.h"
#include "doc/kthumb.h"
#include "glwidget.h"
#include "kdenlivesettings.h"
#include "monitormanager.h"
#include "profiles/profilemodel.hpp"

#include <mlt++/MltConsumer.h>
#include <mlt++/MltFilter.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

MonitorProxy::MonitorProxy(GLWidget *parent)
    : QObject(parent)
    , q(parent)
    , m_position(0)
    , m_zoneIn(0)
    , m_zoneOut(-1)
    , m_hasAV(false)
    , m_clipType(0)
    , m_clipId(-1)
    , m_seekFinished(true)
{
}

int MonitorProxy::getPosition() const
{
    return m_position;
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
    emit q->mouseSeek(delta, modifiers);
}

int MonitorProxy::overlayType() const
{
    return (q->m_id == (int)Kdenlive::ClipMonitor ? KdenliveSettings::clipMonitorOverlayGuides() : KdenliveSettings::projectMonitorOverlayGuides());
}

void MonitorProxy::setOverlayType(int ix)
{
    if (q->m_id == (int)Kdenlive::ClipMonitor) {
        KdenliveSettings::setClipMonitorOverlayGuides(ix);
    } else {
        KdenliveSettings::setProjectMonitorOverlayGuides(ix);
    }
}

QString MonitorProxy::markerComment() const
{
    return m_markerComment;
}

bool MonitorProxy::setPosition(int pos)
{
    if (m_position == pos) {
        return true;
    }
    m_position = pos;
    emit requestSeek(pos);
    if (m_seekFinished) {
        m_seekFinished = false;
        emit seekFinishedChanged();
    }
    emit positionChanged(pos);
    return false;
}

void MonitorProxy::positionFromConsumer(int pos, bool playing)
{
    if (playing) {
        m_position = pos;
        emit positionChanged(pos);
        if (!m_seekFinished) {
            m_seekFinished = true;
            emit seekFinishedChanged();
        }
    } else {
        if (!m_seekFinished && m_position == pos) {
            m_seekFinished = true;
            emit seekFinishedChanged();
        }
    }
}

void MonitorProxy::setMarkerComment(const QString &comment)
{
    if (m_markerComment == comment) {
        return;
    }
    m_markerComment = comment;
    emit markerCommentChanged();
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
        emit removeSnap(m_zoneIn);
    }
    if (pos > m_zoneOut) {
        if (m_zoneOut > 0) {
            emit removeSnap(m_zoneOut - 1);
        }
        m_zoneOut = qMin(q->duration(), pos + (m_zoneOut - m_zoneIn));
        emit addSnap(m_zoneOut - 1);
    }
    m_zoneIn = pos;
    if (pos > 0) {
        emit addSnap(pos);
    }
    emit zoneChanged();
    emit saveZone(QPoint(m_zoneIn, m_zoneOut));
}

void MonitorProxy::setZoneOut(int pos)
{
    if (m_zoneOut > 0) {
        emit removeSnap(m_zoneOut - 1);
    }
    if (pos < m_zoneIn) {
        if (m_zoneIn > 0) {
            emit removeSnap(m_zoneIn);
        }
        m_zoneIn = qMax(0, pos - (m_zoneOut - m_zoneIn));
        emit addSnap(m_zoneIn);
    }
    m_zoneOut = pos;
    if (pos > 0) {
        emit addSnap(m_zoneOut - 1);
    }
    emit zoneChanged();
    emit saveZone(QPoint(m_zoneIn, m_zoneOut));
}

void MonitorProxy::startZoneMove()
{
    m_undoZone = QPoint(m_zoneIn, m_zoneOut);
}

void MonitorProxy::endZoneMove()
{
    emit saveZoneWithUndo(m_undoZone, QPoint(m_zoneIn, m_zoneOut));
}

void MonitorProxy::setZone(int in, int out, bool sendUpdate)
{
    if (m_zoneIn > 0) {
        emit removeSnap(m_zoneIn);
    }
    if (m_zoneOut > 0) {
        emit removeSnap(m_zoneOut - 1);
    }
    m_zoneIn = in;
    m_zoneOut = out;
    if (m_zoneIn > 0) {
        emit addSnap(m_zoneIn);
    }
    if (m_zoneOut > 0) {
        emit addSnap(m_zoneOut - 1);
    }
    emit zoneChanged();
    if (sendUpdate) {
        emit saveZone(QPoint(m_zoneIn, m_zoneOut));
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
}

double MonitorProxy::fps() const
{
    return pCore->getCurrentFps();
}

QPoint MonitorProxy::zone() const
{
    return {m_zoneIn, m_zoneOut};
}

QImage MonitorProxy::extractFrame(int frame_position, const QString &path, int width, int height, bool useSourceProfile)
{
    if (width == -1) {
        width = pCore->getCurrentProfile()->width();
        height = pCore->getCurrentProfile()->height();
    } else if (width % 2 == 1) {
        width++;
    }
    if (!path.isEmpty()) {
        QScopedPointer<Mlt::Profile> tmpProfile(new Mlt::Profile());
        QScopedPointer<Mlt::Producer> producer(new Mlt::Producer(*tmpProfile, path.toUtf8().constData()));
        if (producer && producer->is_valid()) {
            tmpProfile->from_producer(*producer);
            width = tmpProfile->width();
            height = tmpProfile->height();
            double projectFps = pCore->getCurrentFps();
            double currentFps = tmpProfile->fps();
            if (!qFuzzyCompare(projectFps, currentFps)) {
                frame_position = frame_position * currentFps / projectFps;
            }
            QImage img = KThumb::getFrame(producer.data(), frame_position, width, height);
            return img;
        }
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
                tmpProd->seek(q->m_producer->position() * currentFps / projectFps);
            }
            frame = tmpProd->get_frame();
            img = KThumb::getFrame(frame, width, height);
            delete frame;
        }
    } else if (KdenliveSettings::gpu_accel()) {
        QString service = q->m_producer->get("mlt_service");
        QScopedPointer<Mlt::Producer> tmpProd(
            new Mlt::Producer(pCore->getCurrentProfile()->profile(), service.toUtf8().constData(), q->m_producer->get("resource")));
        Mlt::Filter scaler(pCore->getCurrentProfile()->profile(), "swscale");
        Mlt::Filter converter(pCore->getCurrentProfile()->profile(), "avcolor_space");
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

void MonitorProxy::setClipProperties(int clipId, ClipType::ProducerType type, bool hasAV, const QString clipName)
{
    if (clipId != m_clipId) {
        m_clipId = clipId;
        emit clipIdChanged();
    }
    if (hasAV != m_hasAV) {
        m_hasAV = hasAV;
        emit clipHasAVChanged();
    }
    if (type != m_clipType) {
        m_clipType = type;
        emit clipTypeChanged();
    }
    if (clipName == m_clipName) {
        m_clipName.clear();
        emit clipNameChanged();
    }
    m_clipName = clipName;
    emit clipNameChanged();
}

void MonitorProxy::setAudioThumb(const QList <int> streamIndexes, QList <int> channels)
{
    m_audioChannels = channels;
    m_audioStreams = streamIndexes;
    emit audioThumbChanged();
}

void MonitorProxy::setAudioStream(const QString &name)
{
    m_clipStream = name;
    emit clipStreamChanged();
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

bool MonitorProxy::audioThumbFormat() const
{
    return KdenliveSettings::displayallchannels();
}

void MonitorProxy::switchAutoKeyframe()
{
    KdenliveSettings::setAutoKeyframe(!KdenliveSettings::autoKeyframe());
    emit autoKeyframeChanged();
}

bool MonitorProxy::autoKeyframe() const
{
    return KdenliveSettings::autoKeyframe();
}
