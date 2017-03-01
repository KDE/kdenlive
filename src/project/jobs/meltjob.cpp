/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "meltjob.h"
#include "kdenlivesettings.h"
#include "doc/kdenlivedoc.h"

#include <klocalizedstring.h>

#include <mlt++/Mlt.h>

static void consumer_frame_render(mlt_consumer, MeltJob *self, mlt_frame frame_ptr)
{
    Mlt::Frame frame(frame_ptr);
    self->emitFrameNumber((int) frame.get_position());
}

MeltJob::MeltJob(ClipType cType, const QString &id, const QMap<QString, QString> &producerParams, const QMap<QString, QString> &filterParams, const QMap<QString, QString> &consumerParams,  const QMap<QString, QString> &extraParams)
    : AbstractClipJob(MLTJOB, cType, id),
      addClipToProject(0),
      m_consumer(nullptr),
      m_producer(nullptr),
      m_profile(nullptr),
      m_filter(nullptr),
      m_showFrameEvent(nullptr),
      m_producerParams(producerParams),
      m_filterParams(filterParams),
      m_consumerParams(consumerParams),
      m_length(0),
      m_extra(extraParams)
{
    m_jobStatus = JobWaiting;
    description = i18n("Processing clip");
    QString consum = m_consumerParams.value(QStringLiteral("consumer"));
    if (consum.contains(QLatin1Char(':'))) {
        m_dest = consum.section(QLatin1Char(':'), 1);
    }
    m_url = producerParams.value(QStringLiteral("producer"));
}

void MeltJob::startJob()
{
    if (m_url.isEmpty()) {
        m_errorMessage.append(i18n("No producer for this clip."));
        setStatus(JobCrashed);
        return;
    }
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
    Mlt::Profile *projectProfile = new Mlt::Profile(KdenliveSettings::current_profile().toUtf8().constData());
    bool producerProfile = m_extra.contains(QStringLiteral("producer_profile"));
    if (producerProfile) {
        m_profile = new Mlt::Profile;
        m_profile->set_explicit(false);
    } else {
        m_profile = projectProfile;
    }
    if (m_extra.contains(QStringLiteral("resize_profile"))) {
        m_profile->set_height(m_extra.value(QStringLiteral("resize_profile")).toInt());
        m_profile->set_width(m_profile->height() * m_profile->sar());
    }
    double fps = projectProfile->fps();
    int fps_num = projectProfile->frame_rate_num();
    int fps_den = projectProfile->frame_rate_den();
    Mlt::Producer *producer = new Mlt::Producer(*m_profile,  m_url.toUtf8().constData());
    if (producer && producerProfile) {
        m_profile->from_producer(*producer);
        m_profile->set_explicit(true);
    }
    if (qAbs(m_profile->fps() - fps) > 0.01 || producerProfile) {
        // Reload producer
        delete producer;
        // Force same fps as projec profile or the resulting .mlt will not load in our project
        m_profile->set_frame_rate(fps_num, fps_den);
        producer = new Mlt::Producer(*m_profile,  m_url.toUtf8().constData());
    }
    if (producerProfile) {
        delete projectProfile;
    }

    if (!producer || !producer->is_valid()) {
        // Clip was removed or something went wrong, Notify user?
        //m_errorMessage.append(i18n("Invalid clip"));
        setStatus(JobCrashed);
        return;
    }

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

    if (out == -1 && in == -1) {
        m_producer = producer;
    } else {
        m_producer = producer->cut(in, out);
        delete producer;
    }

    // Build consumer
    if (consumerName.contains(QLatin1String(":"))) {
        m_consumer = new Mlt::Consumer(*m_profile, consumerName.section(QLatin1Char(':'), 0, 0).toUtf8().constData(), m_dest.toUtf8().constData());
    } else {
        m_consumer = new Mlt::Consumer(*m_profile, consumerName.toUtf8().constData());
    }
    if (!m_consumer || !m_consumer->is_valid()) {
        m_errorMessage.append(i18n("Cannot create consumer %1.", consumerName));
        setStatus(JobCrashed);
        return;
    }
    if (!m_consumerParams.contains(QStringLiteral("real_time"))) {
        m_consumer->set("real_time", -KdenliveSettings::mltthreads());
    }
    // Process consumer params
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

    // Build filter
    if (!filterName.isEmpty()) {
        m_filter = new Mlt::Filter(*m_profile, filterName.toUtf8().data());
        if (!m_filter || !m_filter->is_valid()) {
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
    Mlt::Tractor tractor(*m_profile);
    Mlt::Playlist playlist;
    playlist.append(*m_producer);
    tractor.set_track(playlist, 0);
    m_consumer->connect(tractor);
    m_producer->set_speed(0);
    m_producer->seek(0);
    m_length = m_producer->get_playtime();
    if (m_length == 0) {
        m_length = m_producer->get_length();
    }
    if (m_filter) {
        m_producer->attach(*m_filter);
    }
    m_showFrameEvent = m_consumer->listen("consumer-frame-render", this, (mlt_listener) consumer_frame_render);
    m_producer->set_speed(1);
    m_consumer->run();

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
}

MeltJob::~MeltJob()
{
    delete m_showFrameEvent;
    delete m_filter;
    delete m_producer;
    delete m_consumer;
    delete m_profile;
}

const QString MeltJob::destination() const
{
    return m_dest;
}

stringMap MeltJob::cancelProperties()
{
    QMap<QString, QString> props;
    return props;
}

const QString MeltJob::statusMessage()
{
    QString statusInfo;
    switch (m_jobStatus) {
    case JobWorking:
        statusInfo = description;
        break;
    case JobWaiting:
        statusInfo = i18n("Waiting to process clip");
        break;
    default:
        break;
    }
    return statusInfo;
}

void MeltJob::emitFrameNumber(int pos)
{
    if (m_length > 0 && m_jobStatus == JobWorking) {
        emit jobProgress(m_clipId, (int)(100 * pos / m_length), jobType);
    }
}

void MeltJob::setStatus(ClipJobStatus status)
{
    m_jobStatus = status;
    if (status == JobAborted && m_consumer) {
        m_consumer->stop();
    }
}

