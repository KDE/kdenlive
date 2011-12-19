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

ProxyJob::ProxyJob(JOBTYPE type, CLIPTYPE cType, const QString &id, QStringList parameters) : AbstractClipJob(type, cType, id, parameters)
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
    if (clipType == PLAYLIST) {
        // change FFmpeg params to MLT format
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

	    QProcess *myProcess = new QProcess;
            myProcess->setProcessChannelMode(QProcess::MergedChannels);
            myProcess->start(KdenliveSettings::rendererpath(), mltParameters);
            myProcess->waitForStarted();
	    return myProcess;
    }
    else if (clipType == IMAGE) {
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
	QStringList parameters;
        parameters << "-i" << m_src;
        QString params = m_proxyParams;
        foreach(QString s, params.split(' '))
        parameters << s;

        // Make sure we don't block when proxy file already exists
        parameters << "-y";
        parameters << m_dest;
        QProcess *myProcess = new QProcess;
        myProcess->setProcessChannelMode(QProcess::MergedChannels);
        myProcess->start("ffmpeg", parameters);
        myProcess->waitForStarted();
	return myProcess;
    }
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


