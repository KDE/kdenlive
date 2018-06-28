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
#include "monitormanager.h"
#include "glwidget.h"
#include "kdenlivesettings.h"
#include "definitions.h"
#include "doc/kthumb.h"

#include <mlt++/MltConsumer.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltFilter.h>

#define SEEK_INACTIVE (-1)

MonitorProxy::MonitorProxy(GLWidget *parent) :
        QObject(parent)
        , q(parent)
        , m_position(0)
        , m_seekPosition(-1)
        , m_zoneIn(0)
        , m_zoneOut(-1)
{
}

int MonitorProxy::seekPosition() const 
{
    return m_seekPosition;
}

bool MonitorProxy::seeking() const
{
    return m_seekPosition != SEEK_INACTIVE;
}

int MonitorProxy::position() const 
{
    return m_position;
}

int MonitorProxy::rulerHeight() const
{
    return q->m_rulerHeight;
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

void MonitorProxy::requestSeekPosition(int pos)
{
    q->activateMonitor();
    m_seekPosition = pos;
    emit seekPositionChanged();
    emit seekRequestChanged();
}

int MonitorProxy::seekOrCurrentPosition() const
{
    return m_seekPosition == SEEK_INACTIVE ? m_position : m_seekPosition;
}

void MonitorProxy::setPosition(int pos, bool *seekStopped)
{
    m_position = pos;
    if (m_seekPosition == m_position) {
        m_seekPosition = SEEK_INACTIVE;
        emit seekPositionChanged();
        *seekStopped = true;
    } else {
        *seekStopped = false;
    }
    emit positionChanged();
}

void MonitorProxy::setMarkerComment(const QString &comment)
{
    if (m_markerComment == comment) {
        return;
    }
    m_markerComment = comment;
    emit markerCommentChanged();
}

void MonitorProxy::setSeekPosition(int pos)
{
    m_seekPosition = pos;
    emit seekPositionChanged();
}

void MonitorProxy::pauseAndSeek(int pos)
{
    q->switchPlay(false);
    requestSeekPosition(pos);
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
    m_zoneIn = pos;
    if (pos > 0) {
        emit addSnap(pos);
    }
    emit zoneChanged();
    emit saveZone();
}

void MonitorProxy::setZoneOut(int pos)
{
    if (m_zoneOut > 0) {
        emit removeSnap(m_zoneOut - 1);
    }
    m_zoneOut = pos;
    if (pos > 0) {
        emit addSnap(pos - 1);
    }
    emit zoneChanged();
    emit saveZone();
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
        emit saveZone();
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

QPoint MonitorProxy::zone() const
{
    return QPoint(m_zoneIn, m_zoneOut);
}

QImage MonitorProxy::extractFrame(int frame_position, const QString &path, int width, int height, bool useSourceProfile)
{
    if (width == -1) {
        width = q->m_monitorProfile->width();
        height = q->m_monitorProfile->height();
    } else if (width % 2 == 1) {
        width++;
    }
    if (!path.isEmpty()) {
        QScopedPointer<Mlt::Producer> producer(new Mlt::Producer(*q->m_monitorProfile, path.toUtf8().constData()));
        if (producer && producer->is_valid()) {
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
            tmpProd->seek(q->m_producer->position());
            frame = tmpProd->get_frame();
            img = KThumb::getFrame(frame, width, height);
            delete frame;
        }
    } else if (KdenliveSettings::gpu_accel()) {
        QString service = q->m_producer->get("mlt_service");
        QScopedPointer<Mlt::Producer> tmpProd(
            new Mlt::Producer(*q->m_monitorProfile, service.toUtf8().constData(), q->m_producer->get("resource")));
        Mlt::Filter scaler(*q->m_monitorProfile, "swscale");
        Mlt::Filter converter(*q->m_monitorProfile, "avcolor_space");
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
