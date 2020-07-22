/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *                                                                         *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "meltjob.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "profiles/profilemodel.hpp"

#include <klocalizedstring.h>

#include <memory>
#include <mlt++/Mlt.h>
static void consumer_frame_render(mlt_consumer, MeltJob *self, mlt_frame frame_ptr)
{
    emit self->jobProgress((int)(100 * mlt_frame_get_position(frame_ptr) / self->length));
}

MeltJob::MeltJob(const QString &binId, JOBTYPE type, bool useProducerProfile, int in, int out)
    : AbstractClipJob(type, binId)
    , m_useProducerProfile(useProducerProfile)
    , m_in(in)
    , m_out(out)
    , m_requiresFilter(true)
{
    if (m_in == -1) {
        m_in = m_inPoint;
    }
    if (m_out == -1) {
        m_out = m_outPoint;
    }
}

bool MeltJob::startJob()
{
    auto binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);
    if (binClip) {
        // Filter applied on a timeline or bin clip
        m_url = binClip->url();
        if (m_url.isEmpty()) {
            m_errorMessage.append(i18n("No producer for this clip."));
            m_successful = false;
            m_done = true;
            return false;
        }

        auto &projectProfile = pCore->getCurrentProfile();
        if (m_useProducerProfile) {
            m_profile.reset(new Mlt::Profile());
            m_profile->set_explicit(0);
        } else {
            m_profile.reset(&projectProfile->profile());
        }
        double fps = projectProfile->fps();
        int fps_num = projectProfile->frame_rate_num();
        int fps_den = projectProfile->frame_rate_den();
        if (KdenliveSettings::gpu_accel()) {
            m_producer = binClip->getClone();
            Mlt::Filter converter(*m_profile.get(), "avcolor_space");
            m_producer->attach(converter);
        } else {
            m_producer = std::make_unique<Mlt::Producer>(*m_profile.get(), m_url.toUtf8().constData());
        }
        if (m_producer && m_useProducerProfile) {
            m_profile->from_producer(*m_producer.get());
            m_profile->set_explicit(1);
            configureProfile();
            if (!qFuzzyCompare(m_profile->fps(), fps)) {
                // Reload producer
                // Force same fps as projec profile or the resulting .mlt will not load in our project
                qDebug()<<"/// FORCING FRAME RATE TO: "<<fps_num<<"\n-------------------";
                m_profile->set_frame_rate(fps_num, fps_den);
                m_producer = std::make_unique<Mlt::Producer>(*m_profile.get(), m_url.toUtf8().constData());
            }
        }
        if ((m_producer == nullptr) || !m_producer->is_valid()) {
            // Clip was removed or something went wrong, Notify user?
            m_errorMessage.append(i18n("Invalid clip"));
            m_successful = false;
            m_done = true;
            return false;
        }
        if (m_out == -1) {
            m_out = m_producer->get_length() - 1;
        }
        if (m_in == -1) {
            m_in = 0;
        }
        if (m_in != 0 || m_out != m_producer->get_length() - 1) {
            std::swap(m_wholeProducer, m_producer);
            m_producer.reset(m_wholeProducer->cut(m_in, m_out));
        }
    } else {
        // Filter applied on a track of master producer, leave config to source job
    }
    if (m_producer != nullptr) {
        length = m_producer->get_playtime();
        if (length == 0) {
            length = m_producer->get_length();
        }
    }

    configureProducer();
    if ((m_producer == nullptr) || !m_producer->is_valid()) {
        // Clip was removed or something went wrong, Notify user?
        m_errorMessage.append(i18n("Invalid clip"));
        m_successful = false;
        m_done = true;
        return false;
    }

    // Build consumer
    configureConsumer();
    /*
    if (m_consumerName.contains(QLatin1String(":"))) {
      m_consumer.reset(new Mlt::Consumer(*m_profile, consumerName.section(QLatin1Char(':'), 0, 0).toUtf8().constData(), m_dest.toUtf8().constData()));
    } else {
        m_consumer = new Mlt::Consumer(*m_profile, consumerName.toUtf8().constData());
        }*/
    if ((m_consumer == nullptr) || !m_consumer->is_valid()) {
        m_errorMessage.append(i18n("Cannot create consumer."));
        m_successful = false;
        m_done = true;
        return false;
    }
    /*
    if (!m_consumerParams.contains(QStringLiteral("real_time"))) {
        m_consumer->set("real_time", -1);
    }
    */

    // Process consumer params
    /*
    QMapIterator<QString, QString> j(m_consumerParams);
    ignoredProps.clear();
    ignoredProps << QStringLiteral("consumer");
    while (j.hasNext()) {
        j.next();
        QString key = j.key();
        if (!ignoredProps.contains(key)) {
            m_consumer->set(j.key().toUtf8().constData(), j.value().toUtf8().constData());
        }
    }
    if (consumerName.startsWith(QStringLiteral("xml:"))) {
        // Use relative path in xml
        m_consumer->set("root", QFileInfo(m_dest).absolutePath().toUtf8().constData());
    }
    */

    // Build filter
    configureFilter();
    /*
    if (!filterName.isEmpty()) {
        m_filter = new Mlt::Filter(*m_profile, filterName.toUtf8().data());
        if ((m_filter == nullptr) || !m_filter->is_valid()) {
            m_errorMessage = i18n("Filter %1 crashed", filterName);
            setStatus(JobCrashed);
            return;
        }

        // Process filter params
        QMapIterator<QString, QString> k(m_filterParams);
        ignoredProps.clear();
        ignoredProps << QStringLiteral("filter");
        while (k.hasNext()) {
            k.next();
            QString key = k.key();
            if (!ignoredProps.contains(key)) {
                m_filter->set(k.key().toUtf8().constData(), k.value().toUtf8().constData());
            }
        }
    }
    */

    if (m_requiresFilter && (m_filter == nullptr || !m_filter->is_valid())) {
        m_errorMessage.append(i18n("Cannot create filter."));
        m_successful = false;
        m_done = true;
        return false;
    }

    Mlt::Tractor tractor(*m_profile.get());
    tractor.set_track(*m_producer.get(), 0);
    m_consumer->connect(tractor);
    m_producer->set_speed(0);
    m_producer->seek(0);
    if (m_filter) {
        m_producer->attach(*m_filter.get());
    }
    m_showFrameEvent.reset(m_consumer->listen("consumer-frame-show", this, (mlt_listener)consumer_frame_render));
    connect(this, &MeltJob::jobCanceled, [&] () {
        m_showFrameEvent.reset();
        m_consumer->stop();
        m_successful = false;
        m_done = true;
        return false;
    });
    m_consumer->run();
    qDebug()<<"===============FILTER PROCESSED\n\n==============0";
    m_successful = m_done = true;
    return true;
}
