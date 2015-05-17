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

#include <QDebug>
#include <QUrl>
#include <klocalizedstring.h>

#include <mlt++/Mlt.h>


static void consumer_frame_render(mlt_consumer, MeltJob * self, mlt_frame frame_ptr)
{
    Mlt::Frame frame(frame_ptr);
    self->emitFrameNumber((int) frame.get_position());
}

MeltJob::MeltJob(ClipType cType, const QString id, const QMap <QString, QString> producerParams, const QMap <QString, QString> filterParams, const QMap <QString, QString> consumerParams,  const QMap <QString, QString>extraParams)
    : AbstractClipJob(MLTJOB, cType, id),
    addClipToProject(0),
    m_consumer(NULL),
    m_producer(NULL),
    m_profile(NULL),
    m_filter(NULL),
    m_showFrameEvent(NULL),
    m_length(0),
    m_producerParams(producerParams),
    m_filterParams(filterParams),
    m_consumerParams(consumerParams),
    m_extra(extraParams)
{
    m_jobStatus = JobWaiting;
    description = i18n("Processing clip");
    QString consum = m_consumerParams.value("consumer");
    if (consum.contains(QLatin1Char(':')))
        m_dest = consum.section(QLatin1Char(':'), 1);
    m_url = producerParams.value("producer");
}

void MeltJob::startJob()
{
    if (m_url.isEmpty()) {
        m_errorMessage.append(i18n("No producer for this clip."));
        setStatus(JobCrashed);
        return;
    }
    int in = m_producerParams.value("in").toInt();
    if (in > 0 && !m_extra.contains(QLatin1String("offset"))) m_extra.insert(QLatin1String("offset"), QString::number(in));
    int out = m_producerParams.value("out").toInt();
    QString filterName = m_filterParams.value("filter");
    QString consumerName = m_consumerParams.value("consumer");
    if (consumerName.contains(QLatin1Char(':'))) m_dest = consumerName.section(QLatin1Char(':'), 1);

    // optional params
    int startPos = -1;
    int track = -1;

    // used when triggering a job from an effect
    if (m_extra.contains("clipStartPos")) startPos = m_extra.value("clipStartPos").toInt();
    if (m_extra.contains("clipTrack")) track = m_extra.value("clipTrack").toInt();

    if (!m_extra.contains(QLatin1String("finalfilter")))
        m_extra.insert(QLatin1String("finalfilter"), filterName);

    if (out != -1 && out <= in) {
        m_errorMessage.append(i18n("Clip zone undefined (%1 - %2).", in, out));
        setStatus(JobCrashed);
        return;
    }
    if (m_extra.contains(QLatin1String("producer_profile"))) {
	m_profile = new Mlt::Profile;
	m_profile->set_explicit(false);
    }
    else {
	m_profile = new Mlt::Profile(KdenliveSettings::current_profile().toUtf8().constData());
    }
    if (m_extra.contains(QLatin1String("resize_profile"))) {
    m_profile->set_height(m_extra.value(QLatin1String("resize_profile")).toInt());
	m_profile->set_width(m_profile->height() * m_profile->sar());
    }
    if (out == -1) {
	m_producer = new Mlt::Producer(*m_profile,  m_url.toUtf8().constData());
	if (m_producer) m_length = m_producer->get_length();
    }
    else {
	Mlt::Producer *tmp = new Mlt::Producer(*m_profile,  m_url.toUtf8().constData());
        if (tmp) m_producer = tmp->cut(in, out);
	delete tmp;
	if (m_producer) m_length = m_producer->get_playtime();
    }
    if (!m_producer || !m_producer->is_valid()) {
	// Clip was removed or something went wrong, Notify user?
	//m_errorMessage.append(i18n("Invalid clip"));
        setStatus(JobCrashed);
	return;
    }
    if (m_extra.contains(QLatin1String("producer_profile"))) {
	m_profile->from_producer(*m_producer);
	m_profile->set_explicit(true);
    }

    // Process producer params
    QMapIterator<QString, QString> i(m_producerParams);
    QStringList ignoredProps;
    ignoredProps << "producer" << "in" << "out";
    while (i.hasNext()) {
	i.next();
	QString key = i.key();
	if (!ignoredProps.contains(key)) {
	    m_producer->set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
	}
    }

    // Build consumer
    if (consumerName.contains(QLatin1String(":"))) {
        m_consumer = new Mlt::Consumer(*m_profile, consumerName.section(QLatin1Char(':'), 0, 0).toUtf8().constData(), m_dest.toUtf8().constData());
    }
    else {
        m_consumer = new Mlt::Consumer(*m_profile, consumerName.toUtf8().constData());
    }
    if (!m_consumer || !m_consumer->is_valid()) {
        m_errorMessage.append(i18n("Cannot create consumer %1.", consumerName));
        setStatus(JobCrashed);
        return;
    }

    m_consumer->set("real_time", -KdenliveSettings::mltthreads() );

    // Process consumer params
    QMapIterator<QString, QString> j(m_consumerParams);
    ignoredProps.clear();
    ignoredProps << "consumer";
    while (j.hasNext()) {
	j.next();
	QString key = j.key();
	if (!ignoredProps.contains(key)) {
	    m_consumer->set(j.key().toUtf8().constData(), j.value().toUtf8().constData());
	}
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
        ignoredProps << "filter";
        while (k.hasNext()) {
            k.next();
            QString key = k.key();
            if (!ignoredProps.contains(key)) {
                m_filter->set(k.key().toUtf8().constData(), k.value().toUtf8().constData());
            }
        }
    }
    Mlt::Tractor tractor;
    Mlt::Playlist playlist;
    playlist.append(*m_producer);
    tractor.set_track(playlist, 0);
    m_consumer->connect(tractor);
    m_producer->set_speed(0);
    m_producer->seek(0);
    if (m_filter) m_producer->attach(*m_filter);
    m_showFrameEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener) consumer_frame_render);
    m_producer->set_speed(1);
    m_consumer->run();

    QMap <QString, QString> jobResults;
    if (m_jobStatus != JobAborted && m_extra.contains(QLatin1String("key"))) {
	QString result = QString::fromLatin1(m_filter->get(m_extra.value(QLatin1String("key")).toUtf8().constData()));
	jobResults.insert(m_extra.value(QLatin1String("key")), result);
    }
    if (!jobResults.isEmpty() && m_jobStatus != JobAborted) {
	emit gotFilterJobResults(m_clipId, startPos, track, jobResults, m_extra);
    }
    if (m_jobStatus == JobWorking) m_jobStatus = JobDone;
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
    QMap <QString, QString> props;
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
    if (m_length > 0 && m_jobStatus != JobAborted) {
        emit jobProgress(m_clipId, (int) (100 * pos / m_length), jobType);
    }
}

bool MeltJob::isProjectFilter() const
{
    return m_extra.contains(QLatin1String("projecttreefilter"));
}

void MeltJob::setStatus(ClipJobStatus status)
{
    m_jobStatus = status;
    if (status == JobAborted && m_consumer) m_consumer->stop();
}



