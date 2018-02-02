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

#include <mlt++/Mlt.h>

static void consumer_frame_render(mlt_consumer, MeltJob *self, mlt_frame frame_ptr)
{
    Mlt::Frame frame(frame_ptr);
    self->mltFrameCallback((int)frame.get_position());
}

MeltJob::MeltJob(const QString &binId, JOBTYPE type, bool useProducerProfile, int in, int out)
    : AbstractClipJob(type, binId)
    , m_profile(pCore->getCurrentProfile()->profile())
    , m_useProducerProfile(useProducerProfile)
    , m_in(in)
    , m_out(out)
{
}

bool MeltJob::startJob()
{
    auto binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);
    m_url = binClip->url();
    if (m_url.isEmpty()) {
        m_errorMessage.append(i18n("No producer for this clip."));
        m_successful = false;
        m_done = true;
        return false;
    }
    /*
    QString consumerName = m_consumerParams.value(QStringLiteral("consumer"));
    // safety check, make sure we don't overwrite a source clip
    if (!m_dest.isEmpty() && !m_dest.endsWith(QStringLiteral(".mlt"))) {
        m_errorMessage.append(i18n("Invalid destination: %1.", consumerName));
        setStatus(JobCrashed);
        return;
    }
    int in = m_producerParams.value(QStringLiteral("in")).toInt();
    if (in > 0 && !m_extra.contains(QStringLiteral("offset"))) {
        m_extra.insert(QStringLiteral("offset"), QString::number(in));
    }
    int out = m_producerParams.value(QStringLiteral("out")).toInt();
    QString filterName = m_filterParams.value(QStringLiteral("filter"));

    // optional params
    int startPos = -1;
    int track = -1;

    // used when triggering a job from an effect
    if (m_extra.contains(QStringLiteral("clipStartPos"))) {
        startPos = m_extra.value(QStringLiteral("clipStartPos")).toInt();
    }
    if (m_extra.contains(QStringLiteral("clipTrack"))) {
        track = m_extra.value(QStringLiteral("clipTrack")).toInt();
    }

    if (!m_extra.contains(QStringLiteral("finalfilter"))) {
        m_extra.insert(QStringLiteral("finalfilter"), filterName);
    }

    if (out != -1 && out <= in) {
        m_errorMessage.append(i18n("Clip zone undefined (%1 - %2).", in, out));
        setStatus(JobCrashed);
        return;
    }
    */
    auto &projectProfile = pCore->getCurrentProfile();
    Mlt::Profile producerProfile;
    // bool producerProfile = m_extra.contains(QStringLiteral("producer_profile"));
    if (m_useProducerProfile) {
        m_profile = producerProfile;
        m_profile.set_explicit(0);
    } else {
        m_profile = projectProfile->profile();
    }
    /*
    if (m_extra.contains(QStringLiteral("resize_profile"))) {
        m_profile->set_height(m_extra.value(QStringLiteral("resize_profile")).toInt());
        m_profile->set_width(m_profile->height() * m_profile->sar());
    }
    */
    double fps = projectProfile->fps();
    int fps_num = projectProfile->frame_rate_num();
    int fps_den = projectProfile->frame_rate_den();

    m_producer.reset(new Mlt::Producer(m_profile, m_url.toUtf8().constData()));
    if (m_producer && m_useProducerProfile) {
        m_profile.from_producer(*m_producer.get());
        m_profile.set_explicit(1);
    }
    configureProfile();
    if (!qFuzzyCompare(m_profile.fps(), fps) || m_useProducerProfile) {
        // Reload producer
        // Force same fps as projec profile or the resulting .mlt will not load in our project
        m_profile.set_frame_rate(fps_num, fps_den);
        m_producer.reset(new Mlt::Producer(m_profile, m_url.toUtf8().constData()));
    }

    if ((m_producer == nullptr) || !m_producer->is_valid()) {
        // Clip was removed or something went wrong, Notify user?
        m_errorMessage.append(i18n("Invalid clip"));
        m_successful = false;
        m_done = true;
        return false;
    }

    /*
    // Process producer params
    QMapIterator<QString, QString> i(m_producerParams);
    QStringList ignoredProps;
    ignoredProps << QStringLiteral("producer") << QStringLiteral("in") << QStringLiteral("out");
    while (i.hasNext()) {
        i.next();
        QString key = i.key();
        if (!ignoredProps.contains(key)) {
            producer->set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
        }
    }
    */

    if (m_out == -1) {
        m_out = m_producer->get_playtime() - 1;
    }
    if (m_in == -1) {
        m_in = 0;
    }
    if (m_out != m_producer->get_playtime() - 1 || m_in != 0) {
        std::swap(m_wholeProducer, m_producer);
        m_producer.reset(m_wholeProducer->cut(m_in, m_out));
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
        m_consumer->set("real_time", -KdenliveSettings::mltthreads());
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
    if ((m_filter == nullptr) || !m_filter->is_valid()) {
        m_errorMessage.append(i18n("Cannot create filter."));
        m_successful = false;
        m_done = true;
        return false;
    }

    Mlt::Tractor tractor(m_profile);
    Mlt::Playlist playlist;
    playlist.append(*m_producer.get());
    tractor.set_track(playlist, 0);
    m_consumer->connect(tractor);
    m_producer->set_speed(0);
    m_producer->seek(0);
    m_length = m_producer->get_playtime();
    if (m_length == 0) {
        m_length = m_producer->get_length();
    }
    if (m_filter) {
        m_producer->attach(*m_filter.get());
    }
    m_showFrameEvent.reset(m_consumer->listen("consumer-frame-render", this, (mlt_listener)consumer_frame_render));
    m_producer->set_speed(1);
    m_consumer->run();

    /*
    QMap<QString, QString> jobResults;
    if (m_jobStatus != JobAborted && m_extra.contains(QStringLiteral("key"))) {
        QString result = QString::fromLatin1(m_filter->get(m_extra.value(QStringLiteral("key")).toUtf8().constData()));
        jobResults.insert(m_extra.value(QStringLiteral("key")), result);
    }
    if (!jobResults.isEmpty() && m_jobStatus != JobAborted) {
        emit gotFilterJobResults(m_clipId, startPos, track, jobResults, m_extra);
    }
    if (m_jobStatus == JobWorking) {
        m_jobStatus = JobDone;
    }
    */
    m_successful = m_done = true;
    return true;
}

void MeltJob::mltFrameCallback(int pos)
{
    if (m_length > 0) {
        emit jobProgress((int)(100 * pos / m_length));
    }
}
