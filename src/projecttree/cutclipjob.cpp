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
    description = i18n("clip cut");
    m_dest = parameters.at(0);
    m_src = parameters.at(1);
    m_start = parameters.at(2);
    m_end = parameters.at(3);
    m_jobDuration = parameters.at(4).toInt();
    addClipToProject = parameters.at(5).toInt();
    if (parameters.count() == 7) m_cutExtraParams = parameters.at(6).simplified();
}

QProcess *CutClipJob::startJob(bool *ok)
{
    // Special case: playlist clips (.mlt or .kdenlive project files)
    if (clipType == AV || clipType == AUDIO || clipType == VIDEO) {
	QStringList parameters;
        parameters << "-i" << m_src;
        parameters << "-ss" << m_start <<"-t" << m_end;
        parameters << "-acodec" << "copy" << "-vcodec" << "copy";
        if (!m_cutExtraParams.isEmpty()) {
            foreach(const QString &s, m_cutExtraParams.split(' '))
                parameters << s;
        }

        // Make sure we don't block when proxy file already exists
        parameters << "-y";
        parameters << m_dest;
        m_jobProcess = new QProcess;
        m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        kDebug()<<"// STARTING CIUT JOS: "<<parameters;
        m_jobProcess->start("ffmpeg", parameters);
        m_jobProcess->waitForStarted();
        QString log = m_jobProcess->readAll();
        if (!log.isEmpty()) m_errorMessage.append(log + '\n');
	return m_jobProcess;
    }
    else m_errorMessage = i18n("Cannot process this clip type.");
    *ok = false;
    return NULL;
}

int CutClipJob::processLogInfo()
{
    if (!m_jobProcess || m_jobDuration == 0) return -1;
    QString log = m_jobProcess->readAll();
    if (!log.isEmpty()) m_errorMessage.append(log + '\n');
    int progress;
    // Parse FFmpeg output
    if (log.contains("frame=")) {
        int progress = log.section("frame=", 1, 1).simplified().section(' ', 0, 0).toInt();
        kDebug()<<"// PROgress: "<<progress<<", DUR: "<<m_jobDuration;
        return (int) (100.0 * progress / m_jobDuration);
    }
    else if (log.contains("time=")) {
        QString time = log.section("time=", 1, 1).simplified().section(' ', 0, 0);
        if (time.contains(':')) {
            QStringList numbers = time.split(':');
            progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
        }
        else progress = (int) time.toDouble();
        kDebug()<<"// PROgress: "<<progress<<", DUR: "<<m_jobDuration;
        return (int) (100.0 * progress / m_jobDuration);
    }
    return -1;
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


