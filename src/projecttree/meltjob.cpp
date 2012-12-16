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
#include "kdenlivedoc.h"

#include <KDebug>
#include <KUrl>
#include <KLocale>

#include <mlt++/Mlt.h>


static void consumer_frame_render(mlt_consumer, MeltJob * self, mlt_frame frame_ptr)
{
    // detect if the producer has finished playing. Is there a better way to do it?
    Mlt::Frame frame(frame_ptr);
    self->emitFrameNumber((int) frame.get_position());
}

MeltJob::MeltJob(CLIPTYPE cType, const QString &id, QStringList parameters,  QMap <QString, QString>extraParams) : AbstractClipJob(MLTJOB, cType, id, parameters),
    addClipToProject(0),
    m_consumer(NULL),
    m_length(0),
    m_extra(extraParams)
{
    m_jobStatus = JOBWAITING;
    m_params = parameters;
    description = i18n("Process clip");
    QString consum = m_params.at(5);
    if (consum.contains(':')) m_dest = consum.section(':', 1);
}

void MeltJob::setProducer(Mlt::Producer *producer, KUrl url)
{
    m_url = QString::fromUtf8(producer->get("resource"));
    if (m_url == "<playlist>" || m_url == "<tractor>" || m_url == "<producer>")
	m_url == url.path();
}

void MeltJob::startJob()
{
    if (m_url.isEmpty()) {
        m_errorMessage.append(i18n("No producer for this clip."));
        setStatus(JOBCRASHED);
        return;
    }
    int in = m_params.takeFirst().toInt();
    if (in > 0 && !m_extra.contains("offset")) m_extra.insert("offset", QString::number(in));
    int out = m_params.takeFirst().toInt();
    QString producerParams =m_params.takeFirst(); 
    QString filter = m_params.takeFirst();
    QString filterParams = m_params.takeFirst();
    QString consumer = m_params.takeFirst();
    if (consumer.contains(':')) m_dest = consumer.section(':', 1);
    QString consumerParams = m_params.takeFirst();
    
    // optional params
    int startPos = -1;
    if (!m_params.isEmpty()) startPos = m_params.takeFirst().toInt();
    int track = -1;
    if (!m_params.isEmpty()) track = m_params.takeFirst().toInt();
    if (!m_extra.contains("finalfilter")) m_extra.insert("finalfilter", filter);

    if (out != -1 && out <= in) {
        m_errorMessage.append(i18n("Clip zone undefined (%1 - %2).", in, out));
        setStatus(JOBCRASHED);
        return;
    }
    Mlt::Producer *prod ;
    if (m_extra.contains("producer_profile")) {
	m_profile = new Mlt::Profile;
	m_profile->set_explicit(false);
    }
    else {
	m_profile = new Mlt::Profile(KdenliveSettings::current_profile().toUtf8().constData());
    }
    if (m_extra.contains("resize_profile")) {	
	m_profile->set_height(m_extra.value("resize_profile").toInt());
	m_profile->set_width(m_profile->height() * m_profile->sar());
    }
    if (out == -1) {
	prod = new Mlt::Producer(*m_profile,  m_url.toUtf8().constData());
	m_length = prod->get_length();
    }
    else {
	Mlt::Producer *tmp = new Mlt::Producer(*m_profile,  m_url.toUtf8().constData());
        prod = tmp->cut(in, out);
	delete tmp;
	m_length = prod->get_playtime();
    }
    if (m_extra.contains("producer_profile")) {
	m_profile->from_producer(*prod);
	m_profile->set_explicit(true);
    }
    QStringList list = producerParams.split(' ', QString::SkipEmptyParts);
    foreach(const QString &data, list) {
        if (data.contains('=')) {
            prod->set(data.section('=', 0, 0).toUtf8().constData(), data.section('=', 1, 1).toUtf8().constData());
        }
    }
    if (consumer.contains(":")) {
        m_consumer = new Mlt::Consumer(*m_profile, consumer.section(':', 0, 0).toUtf8().constData(), consumer.section(':', 1).toUtf8().constData());
    }
    else {
        m_consumer = new Mlt::Consumer(*m_profile, consumer.toUtf8().constData());
    }
    if (!m_consumer || !m_consumer->is_valid()) {
        m_errorMessage.append(i18n("Cannot create consumer %1.", consumer));
        setStatus(JOBCRASHED);
        return;
    }

    //m_consumer->set("terminate_on_pause", 1 );
    //m_consumer->set("eof", "pause" );
    m_consumer->set("real_time", -KdenliveSettings::mltthreads() );


    list = consumerParams.split(' ', QString::SkipEmptyParts);
    foreach(const QString &data, list) {
        if (data.contains('=')) {
            kDebug()<<"// filter con: "<<data;
            m_consumer->set(data.section('=', 0, 0).toUtf8().constData(), data.section('=', 1, 1).toUtf8().constData());
        }
    }
    
    Mlt::Filter mltFilter(*m_profile, filter.toUtf8().data());
    if (!mltFilter.is_valid()) {
	m_errorMessage = i18n("Filter %1 crashed", filter);
        setStatus(JOBCRASHED);
	delete m_consumer;
	delete prod;
	return;
    }
    list = filterParams.split(' ', QString::SkipEmptyParts);
    foreach(const QString &data, list) {
        if (data.contains('=')) {
            kDebug()<<"// filter p: "<<data;
            mltFilter.set(data.section('=', 0, 0).toUtf8().constData(), data.section('=', 1, 1).toUtf8().constData());
        }
    }
    Mlt::Tractor tractor;
    Mlt::Playlist playlist;
    playlist.append(*prod);
    tractor.set_track(playlist, 0);
    m_consumer->connect(tractor);
    prod->set_speed(0);
    prod->seek(0);
    prod->attach(mltFilter);
    Mlt::Event *showFrameEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener) consumer_frame_render);
    prod->set_speed(1);

    m_consumer->run();
    QMap <QString, QString> jobResults;
    if (m_jobStatus != JOBABORTED && m_extra.contains("key")) {
	QString result = mltFilter.get(m_extra.value("key").toUtf8().constData());
	jobResults.insert(m_extra.value("key"), result);
    }
    setStatus(JOBDONE);
    delete prod;
    delete showFrameEvent;
    if (!jobResults.isEmpty() && m_jobStatus != JOBABORTED) emit gotFilterJobResults(m_clipId, startPos, track, jobResults, m_extra);
    return;
}


MeltJob::~MeltJob()
{
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
        case JOBWORKING:
            statusInfo = description;
            break;
        case JOBWAITING:
            statusInfo = i18n("Waiting to process clip");
            break;
        default:
            break;
    }
    return statusInfo;
}

void MeltJob::emitFrameNumber(int pos)
{
    if (m_length > 0) {
        emit jobProgress(m_clipId, (int) (100 * pos / m_length), jobType);
    }
}

bool MeltJob::isProjectFilter() const
{
    return m_extra.contains("projecttreefilter");
}

void MeltJob::setStatus(CLIPJOBSTATUS status)
{
    m_jobStatus = status;
    if (status == JOBABORTED && m_consumer) m_consumer->stop();
}


