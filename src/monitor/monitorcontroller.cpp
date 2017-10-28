/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "monitorcontroller.hpp"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"

#include <mlt++/MltConsumer.h>
#include <mlt++/MltProducer.h>

MonitorController::MonitorController(GLWidget *widget)
    : QObject(widget)
    , m_glWidget(widget)
{
}

void MonitorController::setConsumerProperty(const QString &name, const QString &value)
{
    QMutexLocker locker(&m_glWidget->m_mutex);
    if (m_glWidget->m_consumer) {
        m_glWidget->m_consumer->set(name.toUtf8().constData(), value.toUtf8().constData());
        if (m_glWidget->m_consumer->start() == -1) {
            qCWarning(KDENLIVE_LOG) << "ERROR, Cannot start monitor";
        }
    }
}

QImage MonitorController::extractFrame(int frame_position, const QString &path, int width, int height, bool useSourceProfile)
{
    if (width == -1) {
        width = m_glWidget->m_monitorProfile->width();
        height = m_glWidget->m_monitorProfile->height();
    } else if (width % 2 == 1) {
        width++;
    }
    if (!path.isEmpty()) {
        QScopedPointer<Mlt::Producer> producer(new Mlt::Producer(*m_glWidget->m_monitorProfile, path.toUtf8().constData()));
        if (producer && producer->is_valid()) {
            QImage img = KThumb::getFrame(producer.data(), frame_position, width, height);
            return img;
        }
    }

    if ((m_glWidget->m_producer == nullptr) || !path.isEmpty()) {
        QImage pix(width, height, QImage::Format_RGB32);
        pix.fill(Qt::black);
        return pix;
    }
    Mlt::Frame *frame = nullptr;
    QImage img;
    if (useSourceProfile) {
        // Our source clip's resolution is higher than current profile, export at full res
        QScopedPointer<Mlt::Profile> tmpProfile(new Mlt::Profile());
        QString service = m_glWidget->m_producer->get("mlt_service");
        QScopedPointer<Mlt::Producer> tmpProd(new Mlt::Producer(*tmpProfile, service.toUtf8().constData(), m_glWidget->m_producer->get("resource")));
        tmpProfile->from_producer(*tmpProd);
        width = tmpProfile->width();
        height = tmpProfile->height();
        if (tmpProd && tmpProd->is_valid()) {
            Mlt::Filter scaler(*tmpProfile, "swscale");
            Mlt::Filter converter(*tmpProfile, "avcolor_space");
            tmpProd->attach(scaler);
            tmpProd->attach(converter);
            // TODO: paste effects
            // Clip(*tmpProd).addEffects(*m_glWidget->m_producer);
            tmpProd->seek(m_glWidget->m_producer->position());
            frame = tmpProd->get_frame();
            img = KThumb::getFrame(frame, width, height);
            delete frame;
        }
    } else if (KdenliveSettings::gpu_accel()) {
        QString service = m_glWidget->m_producer->get("mlt_service");
        QScopedPointer<Mlt::Producer> tmpProd(
            new Mlt::Producer(*m_glWidget->m_monitorProfile, service.toUtf8().constData(), m_glWidget->m_producer->get("resource")));
        Mlt::Filter scaler(*m_glWidget->m_monitorProfile, "swscale");
        Mlt::Filter converter(*m_glWidget->m_monitorProfile, "avcolor_space");
        tmpProd->attach(scaler);
        tmpProd->attach(converter);
        tmpProd->seek(m_glWidget->m_producer->position());
        frame = tmpProd->get_frame();
        img = KThumb::getFrame(frame, width, height);
        delete frame;
    } else {
        frame = m_glWidget->m_producer->get_frame();
        img = KThumb::getFrame(frame, width, height);
        delete frame;
    }
    return img;
}
