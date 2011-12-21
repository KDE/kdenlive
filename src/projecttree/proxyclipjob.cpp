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

#include "proxyclipjob.h"
#include "kdenlivesettings.h"
#include "kdenlivedoc.h"

#include <KDebug>
#include <KLocale>

ProxyJob::ProxyJob(CLIPTYPE cType, const QString &id, QStringList parameters) : AbstractClipJob(PROXYJOB, cType, id, parameters),
    m_jobDuration(0),
    m_isFfmpegJob(true)
{
    description = i18n("proxy");
    m_dest = parameters.at(0);
    m_src = parameters.at(1);
    m_exif = parameters.at(2).toInt();
    m_proxyParams = parameters.at(3);
    m_renderWidth = parameters.at(4).toInt();
    m_renderHeight = parameters.at(5).toInt();
}

QProcess *ProxyJob::startJob(bool *ok)
{
    // Special case: playlist clips (.mlt or .kdenlive project files)
    m_jobDuration = 0;
    if (clipType == PLAYLIST) {
        // change FFmpeg params to MLT format
        m_isFfmpegJob = false;
        QStringList mltParameters;
                mltParameters << m_src;
                mltParameters << "-consumer" << "avformat:" + m_dest;
                QStringList params = m_proxyParams.split('-', QString::SkipEmptyParts);
                
                foreach(QString s, params) {
                    s = s.simplified();
                    if (s.count(' ') == 0) {
                        s.append("=1");
                    }
                    else s.replace(' ', '=');
                    mltParameters << s;
                }
        
            mltParameters.append(QString("real_time=-%1").arg(KdenliveSettings::mltthreads()));

            //TODO: currently, when rendering an xml file through melt, the display ration is lost, so we enforce it manualy
            double display_ratio = KdenliveDoc::getDisplayRatio(m_src);
            mltParameters << "aspect=" + QString::number(display_ratio);
            
            // Ask for progress reporting
            mltParameters << "progress=1";

	    m_jobProcess = new QProcess;
            m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
            m_jobProcess->start(KdenliveSettings::rendererpath(), mltParameters);
            m_jobProcess->waitForStarted();
	    return m_jobProcess;
    }
    else if (clipType == IMAGE) {
        m_isFfmpegJob = false;
        // Image proxy
        QImage i(m_src);
        if (i.isNull()) {
	    *ok = false;
	    return NULL;
	}
	
        QImage proxy;
        // Images are scaled to profile size. 
        //TODO: Make it be configurable?
        if (i.width() > i.height()) proxy = i.scaledToWidth(m_renderWidth);
        else proxy = i.scaledToHeight(m_renderHeight);
        if (m_exif > 1) {
            // Rotate image according to exif data
            QImage processed;
            QMatrix matrix;

            switch ( m_exif ) {
                case 2:
		    matrix.scale( -1, 1 );
                    break;
                case 3:
		    matrix.rotate( 180 );
                    break;
                case 4:
		    matrix.scale( 1, -1 );
                    break;
                case 5:
		    matrix.rotate( 270 );
                    matrix.scale( -1, 1 );
                    break;
                case 6:
		    matrix.rotate( 90 );
                    break;
                case 7:
		    matrix.rotate( 90 );
                    matrix.scale( -1, 1 );
                    break;
                case 8:
		    matrix.rotate( 270 );
                    break;
            }
            processed = proxy.transformed( matrix );
            processed.save(m_dest);
        }
        else proxy.save(m_dest);
	*ok = true;
	return NULL;
    }
    else {
        m_isFfmpegJob = true;
	QStringList parameters;
        parameters << "-i" << m_src;
        QString params = m_proxyParams;
        foreach(const QString &s, params.split(' '))
            parameters << s;

        // Make sure we don't block when proxy file already exists
        parameters << "-y";
        parameters << m_dest;
        m_jobProcess = new QProcess;
        m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_jobProcess->start("ffmpeg", parameters);
        m_jobProcess->waitForStarted();
        QString log = m_jobProcess->readAll();
        if (!log.isEmpty()) m_errorMessage.append(log + '\n');
	return m_jobProcess;
    }
}

int ProxyJob::processLogInfo()
{
    if (!m_jobProcess) return -1;
    QString log = m_jobProcess->readAll();
    if (!log.isEmpty()) m_errorMessage.append(log + '\n');
    int progress;
    if (m_isFfmpegJob) {
        // Parse FFmpeg output
        if (m_jobDuration == 0) {
            if (log.contains("Duration:")) {
                QString data = log.section("Duration:", 1, 1).section(',', 0, 0).simplified();
                QStringList numbers = data.split(':');
                m_jobDuration = (int) (numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble());
            }
        }
        else if (log.contains("time=")) {
            QString time = log.section("time=", 1, 1).simplified().section(' ', 0, 0);
            if (time.contains(':')) {
                QStringList numbers = time.split(':');
                progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
            }
            else progress = (int) time.toDouble();
            return (int) (100.0 * progress / m_jobDuration);
        }
    }
    else {
        // Parse MLT output
        if (log.contains("percentage:")) {
            progress = log.section(':', -1).simplified().toInt();
            return progress;
        }
    }
    return -1;
}

ProxyJob::~ProxyJob()
{
}

const QString ProxyJob::destination() const
{
    return m_dest;
}

stringMap ProxyJob::cancelProperties()
{
    QMap <QString, QString> props;
    props.insert("proxy", "-");
    return props;
}

const QString ProxyJob::statusMessage(CLIPJOBSTATUS status)
{
    QString statusInfo;
    switch (status) {
        case CREATINGJOB:
            statusInfo = i18n("Creating proxy");
            break;
        case JOBWAITING:
            statusInfo = i18n("Waiting - proxy");
            break;
        default:
            break;
    }
    return statusInfo;
}

