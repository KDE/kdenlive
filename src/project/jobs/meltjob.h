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

#ifndef MELTJOB
#define MELTJOB

#include "abstractclipjob.h"

namespace Mlt
{
class Profile;
class Producer;
class Consumer;
class Filter;
class Event;
}

/**
 * @class MeltJob
 * @brief This class contains a Job that will run an MLT Producer, with some optional filter
 *
 */

class MeltJob : public AbstractClipJob
{
    Q_OBJECT

public:
    /** @brief Creates the Job.
     *  @param cType the Clip Type (AV, PLAYLIST, AUDIO, ...) as defined in definitions.h. Some jobs will act differently depending on clip type
     *  @param id the id of the clip that requested this clip job
     *  @param producerParams the parameters that will be passed to the Mlt::Producer. The "producer" value will be used to initialize the producer.
     *         should contain the in and out values (in=0, out=-1 to process all clip)
     *  @param filterParams the parameters that will be passed to the (optional) Mlt::Filter attached to the producer. The "filter" value
     *         should contain the MLT's filter name.
     *  @param consumerParams the parameters passed to the Mlt::Consumer. The "consumer" value should hold the consumer's initialization string.
        @param extraParams these parameters can be used to further affect the Job handling.
     */
    MeltJob(ClipType cType, const QString &id,  const QMap<QString, QString> &producerParams, const QMap<QString, QString> &filterParams, const QMap<QString, QString> &consumerParams, const stringMap &extraParams = stringMap());
    virtual ~ MeltJob();
    /** @brief Returns the file path that will be written by this Mlt job. Empty when no file is written. */
    const QString destination() const Q_DECL_OVERRIDE;
    /** @brief Start processing the job. */
    void startJob() Q_DECL_OVERRIDE;
    /** @brief These properties can be used to undo the action that launched this job. */
    stringMap cancelProperties() Q_DECL_OVERRIDE;
    /** @brief When true, this will tell the JobManager to add the @destination() file to the project Bin. */
    bool addClipToProject;
    /** @brief Returns a text string describing the job's current activity. */
    const QString statusMessage() Q_DECL_OVERRIDE;
    /** @brief Sets the status for this job (can be used by the JobManager to abort the job). */
    void setStatus(ClipJobStatus status) Q_DECL_OVERRIDE;
    /** @brief Here we will send the current progress info to anyone interested. */
    void emitFrameNumber(int pos);

private:
    Mlt::Consumer *m_consumer;
    Mlt::Producer *m_producer;
    Mlt::Profile *m_profile;
    Mlt::Filter *m_filter;
    Mlt::Event *m_showFrameEvent;
    QMap<QString, QString> m_producerParams;
    QMap<QString, QString> m_filterParams;
    QMap<QString, QString> m_consumerParams;
    QString m_dest;
    QString m_url;
    int m_length;
    QMap<QString, QString> m_extra;

signals:
    /** @brief When user requested a to process an Mlt::Filter, this will send back all necessary infos. */
    void gotFilterJobResults(const QString &id, int startPos, int track, const stringMap &result, const stringMap &extra);
};

#endif
