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
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"

#include <QProcess>
#include <QTemporaryFile>
#include <QProcess>

#include <klocalizedstring.h>


ProxyJob::ProxyJob(const QString &binId)
    : AbstractClipJob(PROXYJOB, binId)
    , m_jobDuration(0)
    , m_isFfmpegJob(true)
    , m_done(false)
{
}

const QString ProxyJob::getDescription() const
{
    return i18n("Creating proxy %1", m_clipId);
}


bool ProxyJob::startJob()
{
    auto binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);
    const QString dest = binClip->getProducerProperty(QStringLiteral("kdenlive:proxy"));
    if (QFile::exists(dest)) {
        // Proxy clip already created
        m_done = true;
        return true;
    }
    ClipType type = binClip->clipType();
    bool result;
    QString source = binClip->getProducerProperty(QStringLiteral("kdenlive:originalurl"));
    int exif = binClip->getProducerIntProperty(QStringLiteral("_exif_orientation"));
    if (type == ClipType::Playlist || type == ClipType::SlideShow) {
        // change FFmpeg params to MLT format
        m_isFfmpegJob = false;
        QStringList mltParameters;
        QTemporaryFile *playlist = nullptr;
        // set clip origin
        if (type == ClipType::Playlist) {
            // Special case: playlists use the special 'consumer' producer to support resizing
            source.prepend(QStringLiteral("consumer:"));
        } else {
            // create temporary playlist to generate proxy
            // we save a temporary .mlt clip for rendering
            QDomDocument doc;
            QDomElement xml = binClip->toXml(doc, false);
            playlist = new QTemporaryFile();
            playlist->setFileTemplate(playlist->fileTemplate() + QStringLiteral(".mlt"));
            if (playlist->open()) {
                source = playlist->fileName();
                QTextStream out(playlist);
                out << doc.toString();
                playlist->close();
            }
        }
        mltParameters << source;
        // set destination
        mltParameters << QStringLiteral("-consumer") << QStringLiteral("avformat:") + dest;
        QString parameter = pCore->currentDoc()->getDocumentProperty(QStringLiteral("proxyparams")).simplified();
        QStringList params = parameter.split(QLatin1Char('-'), QString::SkipEmptyParts);
        double display_ratio;
        if (source.startsWith(QLatin1String("consumer:"))) {
            display_ratio = KdenliveDoc::getDisplayRatio(source.section(QLatin1Char(':'), 1));
        } else {
            display_ratio = KdenliveDoc::getDisplayRatio(source);
        }
        if (display_ratio < 1e-6) {
            display_ratio = 1;
        }

        bool skipNext = false;
        for (const QString &s : params) {
            QString t = s.simplified();
            if (skipNext) {
                skipNext = false;
                continue;
            }
            if (t.count(QLatin1Char(' ')) == 0) {
                t.append(QLatin1String("=1"));
            } else if (t.startsWith(QLatin1String("vf "))) {
                skipNext = true;
                bool ok = false;
                int width = t.section(QLatin1Char('='), 1, 1).section(QLatin1Char(':'), 0, 0).toInt(&ok);
                if (!ok) {
                    width = 640;
                }
                int height = width / display_ratio;
                // Make sure we get an even height
                height += height % 2;
                mltParameters << QStringLiteral("s=%1x%2").arg(width).arg(height);
                if (t.contains(QStringLiteral("yadif"))) {
                    mltParameters << QStringLiteral("progressive=1");
                }
                continue;
            } else {
                t.replace(QLatin1Char(' '), QLatin1String("="));
            }
            mltParameters << t;
        }

        mltParameters.append(QStringLiteral("real_time=-%1").arg(KdenliveSettings::mltthreads()));

        // TODO: currently, when rendering an xml file through melt, the display ration is lost, so we enforce it manualy
        mltParameters << QStringLiteral("aspect=") + QLocale().toString(display_ratio);

        // Ask for progress reporting
        mltParameters << QStringLiteral("progress=1");

        m_jobProcess = new QProcess;
        m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_jobProcess, &QProcess::readyReadStandardOutput, this, &ProxyJob::processLogInfo);
        m_jobProcess->start(KdenliveSettings::rendererpath(), mltParameters);
        m_jobProcess->waitForFinished(-1);
        result = m_jobProcess->exitStatus() == QProcess::NormalExit;
        delete playlist;
    } else if (type == ClipType::Image) {
        m_isFfmpegJob = false;
        // Image proxy
        QImage i(source);
        if (i.isNull()) {
            m_done = false;
            m_errorMessage.append(i18n("Cannot load image %1.", source));
            return false;
        }

        QImage proxy;
        // Images are scaled to profile size.
        // TODO: Make it be configurable?
        if (i.width() > i.height()) {
            proxy = i.scaledToWidth(KdenliveSettings::proxyimagesize());
        } else {
            proxy = i.scaledToHeight(KdenliveSettings::proxyimagesize());
        }
        if (exif > 1) {
            // Rotate image according to exif data
            QImage processed;
            QMatrix matrix;

            switch (exif) {
            case 2:
                matrix.scale(-1, 1);
                break;
            case 3:
                matrix.rotate(180);
                break;
            case 4:
                matrix.scale(1, -1);
                break;
            case 5:
                matrix.rotate(270);
                matrix.scale(-1, 1);
                break;
            case 6:
                matrix.rotate(90);
                break;
            case 7:
                matrix.rotate(90);
                matrix.scale(-1, 1);
                break;
            case 8:
                matrix.rotate(270);
                break;
            }
            processed = proxy.transformed(matrix);
            processed.save(dest);
        } else {
            proxy.save(dest);
        }
        m_done = true;
        return true;
    } else {
        m_isFfmpegJob = true;
        QStringList parameters;
        if (KdenliveSettings::ffmpegpath().isEmpty()) {
            // FFmpeg not detected, cannot process the Job
            m_errorMessage.prepend(i18n("Failed to create proxy. FFmpeg not found, please set path in Kdenlive's settings Environment"));
            m_done = true;
            return false;
        }
        const QString proxyParams = pCore->currentDoc()->getDocumentProperty(QStringLiteral("proxyparams")).simplified();
        if (proxyParams.contains(QStringLiteral("-noautorotate"))) {
            // The noautorotate flag must be passed before input source
            parameters << QStringLiteral("-noautorotate");
        }
        if (proxyParams.contains(QLatin1String("-i "))) {
            // we have some pre-filename parameters, filename will be inserted later
        } else {
            parameters << QStringLiteral("-i") << source;
        }
        QString params = proxyParams;
        for (const QString &s : params.split(QLatin1Char(' '))) {
            QString t = s.simplified();
            if (t != QLatin1String("-noautorotate")) {
                parameters << t;
                if (t == QLatin1String("-i")) {
                    parameters << source;
                }
            }
        }

        // Make sure we don't block when proxy file already exists
        parameters << QStringLiteral("-y");
        parameters << dest;
        m_jobProcess = new QProcess;
        m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_jobProcess, &QProcess::readyReadStandardOutput, this, &ProxyJob::processLogInfo);
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters, QIODevice::ReadOnly);
        m_jobProcess->waitForFinished(-1);
        result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    }
    // remove temporary playlist if it exists
    if (result) {
        if (QFileInfo(dest).size() == 0) {
            // File was not created
            m_done = false;
            m_errorMessage.append(i18n("Failed to create proxy clip."));
        } else {
            m_done = true;
        }
    } else {
        // Proxy process crashed
        QFile::remove(dest);
        m_done = false;
        m_errorMessage.append(QString::fromUtf8(m_jobProcess->readAll()));
    }
    delete m_jobProcess;
    return result;
}

void ProxyJob::processLogInfo()
{
    int progress;
    const QString log = QString::fromUtf8(m_jobProcess->readAll());
    if (m_isFfmpegJob) {
        // Parse FFmpeg output
        if (m_jobDuration == 0) {
            if (log.contains(QLatin1String("Duration:"))) {
                QString data = log.section(QStringLiteral("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
                QStringList numbers = data.split(QLatin1Char(':'));
                m_jobDuration = (int)(numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble());
            }
        } else if (log.contains(QLatin1String("time="))) {
            QString time = log.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
            if (time.contains(QLatin1Char(':'))) {
                QStringList numbers = time.split(QLatin1Char(':'));
                progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
            } else {
                progress = (int)time.toDouble();
            }
            emit jobProgress((int)(100.0 * progress / m_jobDuration));
        }
    } else {
        // Parse MLT output
        if (log.contains(QLatin1String("percentage:"))) {
            progress = log.section(QStringLiteral("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
            emit jobProgress(progress);
        }
    }
}

bool ProxyJob::commitResult(Fun &undo, Fun &redo)
{
    Q_ASSERT(!m_resultConsumed);
    if (!m_done) {
        qDebug() << "ERROR: Trying to consume invalid results";
        auto binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);
        binClip->setProducerProperty(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
        return false;
    }
    m_resultConsumed = true;
    auto operation = [ clipId = m_clipId ]()
    {
        auto binClip = pCore->projectItemModel()->getClipByBinID(clipId);
        const QString dest = binClip->getProducerProperty(QStringLiteral("kdenlive:proxy"));
        binClip->setProducerProperty(QStringLiteral("resource"), dest);
        pCore->bin()->reloadClip(clipId);
        return true;
    };
    auto reverse = [ clipId = m_clipId ]()
    {
        auto binClip = pCore->projectItemModel()->getClipByBinID(clipId);
        const QString dest = binClip->getProducerProperty(QStringLiteral("kdenlive:originalurl"));
        binClip->setProducerProperty(QStringLiteral("resource"), dest);
        pCore->bin()->reloadClip(clipId);
        return true;
    };
    bool ok = operation();
    if (ok) {
        UPDATE_UNDO_REDO_NOLOCK(operation, reverse, undo, redo);
    }
    return ok;
    return true;
}
