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
#include <KLocale>

#include <mlt++/Mlt.h>


static void consumer_frame_render(mlt_consumer, MeltJob * self, mlt_frame /*frame_ptr*/)
{
    // detect if the producer has finished playing. Is there a better way to do it?
    self->emitFrameNumber();
}

MeltJob::MeltJob(CLIPTYPE cType, const QString &id, QStringList parameters) : AbstractClipJob(MLTJOB, cType, id, parameters), 
    m_producer(NULL),
    m_profile(NULL),
    m_consumer(NULL),
    m_length(0)
{
    jobStatus = JOBWAITING;
    m_params = parameters;
    description = i18n("Process clip");
}

void MeltJob::setProducer(Mlt::Producer *producer)
{
    m_producer = producer;
}

void MeltJob::startJob()
{
    if (!m_producer) {
        m_errorMessage.append(i18n("No producer for this clip."));
        setStatus(JOBCRASHED);
        return;
    }
    QString filter = m_params.takeFirst();
    QString filterParams = m_params.takeFirst();
    QString consumer = m_params.takeFirst();
    QString consumerParams = m_params.takeFirst();
    QString properties = m_params.takeFirst();
    m_profile = m_producer->profile();
    m_consumer = new Mlt::Consumer(*m_profile, consumer.toUtf8().constData());
    if (!m_consumer || !m_consumer->is_valid()) {
        m_errorMessage.append(i18n("Cannot create consumer %1.", consumer));
        setStatus(JOBCRASHED);
        return;
    }

    m_consumer->set("terminate_on_pause", 1 );
    m_consumer->set("eof", "pause" );
    m_consumer->set("real_time", -1 );

    QStringList list = consumerParams.split(' ', QString::SkipEmptyParts);
    foreach(QString data, list) {
        if (data.contains('=')) {
            m_consumer->set(data.section('=', 0, 0).toUtf8().constData(), data.section('=', 1, 1).toUtf8().constData());
        }
    }
    
    Mlt::Filter mltFilter(*m_profile, filter.toUtf8().data());
    list = filterParams.split(' ', QString::SkipEmptyParts);
    foreach(QString data, list) {
        if (data.contains('=')) {
            mltFilter.set(data.section('=', 0, 0).toUtf8().constData(), data.section('=', 1, 1).toUtf8().constData());
        }
    }
    m_producer->attach(mltFilter);
    m_length = m_producer->get_length();
    m_consumer->connect(*m_producer);
    m_producer->set_speed(0);
    m_producer->seek(0);
    m_showFrameEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener) consumer_frame_render);
    m_consumer->start();
    m_producer->set_speed(1);
    while (jobStatus != JOBABORTED && !m_consumer->is_stopped()) {
        
    }
    m_consumer->stop();
    QStringList wanted = properties.split(',', QString::SkipEmptyParts);
    foreach(const QString key, wanted) {
        QString value = mltFilter.get(key.toUtf8().constData());
        kDebug()<<"RESULT: "<<key<<" = "<< value;
    }
    setStatus(JOBDONE);
    delete m_consumer;
    return;
}


MeltJob::~MeltJob()
{
}

const QString MeltJob::destination() const
{
    return QString();
}

stringMap MeltJob::cancelProperties()
{
    QMap <QString, QString> props;
    return props;
}

const QString MeltJob::statusMessage()
{
    QString statusInfo;
    switch (jobStatus) {
        case JOBWORKING:
            statusInfo = i18n("Processing clip");
            break;
        case JOBWAITING:
            statusInfo = i18n("Waiting - process clip");
            break;
        default:
            break;
    }
    return statusInfo;
}

void MeltJob::emitFrameNumber()
{
    if (m_consumer && m_length > 0) {
        emit jobProgress(m_clipId, (int) (100 * m_consumer->position() / m_length), jobType);
    }
}