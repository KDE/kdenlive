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

#include "cutclipjob.h"
#include "kdenlivesettings.h"
#include "kdenlivedoc.h"

#include <KDebug>
#include <KLocale>

CutClipJob::CutClipJob(CLIPTYPE cType, const QString &id, QStringList parameters) : AbstractClipJob(CUTJOB, cType, id, parameters)
{
    jobStatus = JOBWAITING;
    m_dest = parameters.at(0);
    m_src = parameters.at(1);
    m_start = parameters.at(2);
    m_end = parameters.at(3);
    if (m_start.isEmpty()) {
        // this is a transcoding job
        description = i18n("Transcode clip");
    } else {
        description = i18n("Cut clip");
    }
    m_jobDuration = parameters.at(4).toInt();
    m_addClipToProject = parameters.at(5).toInt();
    replaceClip = false;
    if (parameters.count() == 7) m_cutExtraParams = parameters.at(6).simplified();
}

void CutClipJob::startJob()
{
    // Special case: playlist clips (.mlt or .kdenlive project files)
    if (clipType == AV || clipType == AUDIO || clipType == VIDEO) {
        QStringList parameters;
        parameters << "-i" << m_src;
        if (!m_start.isEmpty())
            parameters << "-ss" << m_start <<"-t" << m_end;
        if (!m_cutExtraParams.isEmpty()) {
            foreach(const QString &s, m_cutExtraParams.split(' '))
                parameters << s;
        }

        // Make sure we don't block when proxy file already exists
        parameters << "-y";
        parameters << m_dest;
        m_jobProcess = new QProcess;
        m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters);
        m_jobProcess->waitForStarted();
        while (m_jobProcess->state() != QProcess::NotRunning) {
            processLogInfo();
            if (jobStatus == JOBABORTED) {
                m_jobProcess->close();
                m_jobProcess->waitForFinished();
                QFile::remove(m_dest);
            }
            m_jobProcess->waitForFinished(400);
        }
        
        if (jobStatus != JOBABORTED) {
            int result = m_jobProcess->exitStatus();
            if (result == QProcess::NormalExit) {
                if (QFileInfo(m_dest).size() == 0) {
                    // File was not created
                    processLogInfo();
                    m_errorMessage.append(i18n("Failed to create file."));
                    setStatus(JOBCRASHED);
                } else {
                    setStatus(JOBDONE);
                }
            } else if (result == QProcess::CrashExit) {
                // Proxy process crashed
                QFile::remove(m_dest);
                setStatus(JOBCRASHED);
            }
        }
        delete m_jobProcess;
        return;
    } else {
        m_errorMessage = i18n("Cannot process this clip type.");
    }
    setStatus(JOBCRASHED);
    return;
}

void CutClipJob::processLogInfo()
{
    if (!m_jobProcess || m_jobDuration == 0 || jobStatus == JOBABORTED) return;
    QString log = m_jobProcess->readAll();
    if (!log.isEmpty()) m_logDetails.append(log + '\n');
    int progress;
    // Parse FFmpeg output
    if (log.contains("frame=")) {
        progress = log.section("frame=", 1, 1).simplified().section(' ', 0, 0).toInt();
        emit jobProgress(m_clipId, (int) (100.0 * progress / m_jobDuration), jobType);
    }
    else if (log.contains("time=")) {
        QString time = log.section("time=", 1, 1).simplified().section(' ', 0, 0);
        if (time.contains(':')) {
            QStringList numbers = time.split(':');
            progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
        }
        else progress = (int) time.toDouble();
        emit jobProgress(m_clipId, (int) (100.0 * progress / m_jobDuration), jobType);
    }
}

CutClipJob::~CutClipJob()
{
}

const QString CutClipJob::destination() const
{
    return m_dest;
}

stringMap CutClipJob::cancelProperties()
{
    QMap <QString, QString> props;
    return props;
}

const QString CutClipJob::statusMessage()
{
    QString statusInfo;
    switch (jobStatus) {
        case JOBWORKING:
            if (m_start.isEmpty()) statusInfo = i18n("Transcoding clip");
            else statusInfo = i18n("Extracting clip cut");
            break;
        case JOBWAITING:
            if (m_start.isEmpty()) statusInfo = i18n("Waiting - transcode clip");
            else statusInfo = i18n("Waiting - cut clip");
            break;
        default:
            break;
    }
    return statusInfo;
}

bool CutClipJob::isExclusive()
{
    return false;
}

