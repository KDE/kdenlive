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
#include "kdenlive_debug.h"
#include "doc/kdenlivedoc.h"
#include "bin/projectclip.h"
#include "bin/bin.h"
#include <QProcess>
#include <QTemporaryFile>

#include <klocalizedstring.h>

ProxyJob::ProxyJob(ClipType cType, const QString &id, const QStringList &parameters, QTemporaryFile *playlist)
    : AbstractClipJob(PROXYJOB, cType, id),
      m_jobDuration(0),
      m_isFfmpegJob(true)
{
    m_jobStatus = JobWaiting;
    description = i18n("proxy");
    m_dest = parameters.at(0);
    m_src = parameters.at(1);
    m_exif = parameters.at(2).toInt();
    m_proxyParams = parameters.at(3);
    m_renderWidth = parameters.at(4).toInt();
    m_renderHeight = parameters.at(5).toInt();
    m_playlist = playlist;
    replaceClip = true;
}

void ProxyJob::startJob()
{
    // Special case: playlist clips (.mlt or .kdenlive project files)
    m_jobDuration = 0;
    if (clipType == Playlist || clipType == SlideShow) {
        // change FFmpeg params to MLT format
        m_isFfmpegJob = false;
        QStringList mltParameters;
        mltParameters << m_src;
        mltParameters << QStringLiteral("-consumer") << QStringLiteral("avformat:") + m_dest;
        QStringList params = m_proxyParams.split(QLatin1Char('-'), QString::SkipEmptyParts);
        double display_ratio;
        if (m_src.startsWith(QLatin1String("consumer:"))) {
            display_ratio = KdenliveDoc::getDisplayRatio(m_src.section(QLatin1Char(':'), 1));
        } else {
            display_ratio = KdenliveDoc::getDisplayRatio(m_src);
        }
        if (display_ratio < 1e-6) {
            display_ratio = 1;
        }

        bool skipNext = false;
        foreach (const QString &s, params) {
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
                mltParameters << QStringLiteral("s=%1x%2").arg(width).arg(height);
                continue;
            } else {
                t.replace(QLatin1Char(' '), QLatin1String("="));
            }
            mltParameters << t;
        }

        mltParameters.append(QStringLiteral("real_time=-%1").arg(KdenliveSettings::mltthreads()));

        //TODO: currently, when rendering an xml file through melt, the display ration is lost, so we enforce it manualy
        mltParameters << QStringLiteral("aspect=") + QLocale().toString(display_ratio);

        // Ask for progress reporting
        mltParameters << QStringLiteral("progress=1");

        m_jobProcess = new QProcess;
        m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_jobProcess->start(KdenliveSettings::rendererpath(), mltParameters);
        m_jobProcess->waitForStarted();
    } else if (clipType == Image) {
        m_isFfmpegJob = false;
        // Image proxy
        QImage i(m_src);
        if (i.isNull()) {
            m_errorMessage.append(i18n("Cannot load image %1.", m_src));
            setStatus(JobCrashed);
            return;
        }

        QImage proxy;
        // Images are scaled to profile size.
        //TODO: Make it be configurable?
        if (i.width() > i.height()) {
            proxy = i.scaledToWidth(m_renderWidth);
        } else {
            proxy = i.scaledToHeight(m_renderHeight);
        }
        if (m_exif > 1) {
            // Rotate image according to exif data
            QImage processed;
            QMatrix matrix;

            switch (m_exif) {
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
            processed.save(m_dest);
        } else {
            proxy.save(m_dest);
        }
        setStatus(JobDone);
        return;
    } else {
        m_isFfmpegJob = true;
        if (KdenliveSettings::ffmpegpath().isEmpty()) {
            //FFmpeg not detected, cannot process the Job
            m_errorMessage.prepend(i18n("Failed to create proxy. FFmpeg not found, please set path in Kdenlive's settings Environment"));
            setStatus(JobCrashed);
            return;
        }
        QStringList parameters;
        if (m_proxyParams.contains(QStringLiteral("-noautorotate"))) {
            // The noautorotate flag must be passed before input source
            parameters << QStringLiteral("-noautorotate");
        }
        if (m_proxyParams.contains(QLatin1String("-i "))) {
            // we have some pre-filename parameters, filename will be inserted later
        } else {
            parameters << QStringLiteral("-i") << m_src;
        }
        QString params = m_proxyParams;
        foreach (const QString &s, params.split(QLatin1Char(' '))) {
            QString t = s.simplified();
            if (t != QLatin1String("-noautorotate")) {
                parameters << t;
                if (t == QLatin1String("-i")) {
                    parameters << m_src;
                }
            }
        }

        // Make sure we don't block when proxy file already exists
        parameters << QStringLiteral("-y");
        parameters << m_dest;
        m_jobProcess = new QProcess;
        m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters, QIODevice::ReadOnly);
        m_jobProcess->waitForStarted();
    }
    while (m_jobProcess->state() != QProcess::NotRunning) {
        processLogInfo();
        if (m_jobStatus == JobAborted) {
            emit cancelRunningJob(m_clipId, cancelProperties());
            m_jobProcess->close();
            m_jobProcess->waitForFinished();
            QFile::remove(m_dest);
        }
        m_jobProcess->waitForFinished(400);
    }
    // remove temporary playlist if it exists
    delete m_playlist;
    if (m_jobStatus != JobAborted) {
        int result = m_jobProcess->exitStatus();
        if (result == QProcess::NormalExit) {
            if (QFileInfo(m_dest).size() == 0) {
                // File was not created
                processLogInfo();
                m_errorMessage.append(i18n("Failed to create proxy clip."));
                setStatus(JobCrashed);
            } else {
                setStatus(JobDone);
            }
        } else if (result == QProcess::CrashExit) {
            // Proxy process crashed
            QFile::remove(m_dest);
            setStatus(JobCrashed);
        }
    }
    delete m_jobProcess;
}

void ProxyJob::processLogInfo()
{
    if (!m_jobProcess || m_jobStatus == JobAborted) {
        return;
    }
    QString log = QString::fromUtf8(m_jobProcess->readAll());
    if (!log.isEmpty()) {
        m_logDetails.append(log + QLatin1Char('\n'));
    } else {
        return;
    }

    int progress;
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
                progress = (int) time.toDouble();
            }
            emit jobProgress(m_clipId, (int)(100.0 * progress / m_jobDuration), jobType);
        }
    } else {
        // Parse MLT output
        if (log.contains(QLatin1String("percentage:"))) {
            progress = log.section(QStringLiteral("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
            emit jobProgress(m_clipId, progress, jobType);
        }
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
    QMap<QString, QString> props;
    props.insert(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
    return props;
}

const QString ProxyJob::statusMessage()
{
    QString statusInfo;
    switch (m_jobStatus) {
    case JobWorking:
        statusInfo = i18n("Creating proxy");
        break;
    case JobWaiting:
        statusInfo = i18n("Waiting - proxy");
        break;
    default:
        break;
    }
    return statusInfo;
}

// static
QList<ProjectClip *> ProxyJob::filterClips(const QList<ProjectClip *> &clips)
{
    QList<ProjectClip *> result;
    for (int i = 0; i < clips.count(); i++) {
        ProjectClip *clip = clips.at(i);
        ClipType type = clip->clipType();
        if (type != AV && type != Video && type != Playlist && type != Image && type != SlideShow) {
            // Clip will not be processed by this job
            continue;
        }
        result << clip;
    }
    return result;
}

// static
QHash<ProjectClip *, AbstractClipJob *> ProxyJob::prepareJob(Bin *bin, const QList<ProjectClip *> &clips)
{
    QHash<ProjectClip *, AbstractClipJob *> jobs;
    QSize renderSize = bin->getRenderSize();
    QString params = bin->getDocumentProperty(QStringLiteral("proxyparams")).simplified();
    for (int i = 0; i < clips.count(); i++) {
        ProjectClip *item = clips.at(i);
        QString id = item->clipId();
        QString path = item->getProducerProperty(QStringLiteral("kdenlive:proxy"));
        if (path.isEmpty()) {
            item->setJobStatus(AbstractClipJob::PROXYJOB, JobCrashed, -1, i18n("Failed to create proxy, empty path."));
            continue;
        }
        // Reset proxy path until it is really created
        item->setProducerProperty(QStringLiteral("kdenlive:proxy"), QString());
        if (QFileInfo(path).size() > 0) {
            // Proxy already created
            item->setJobStatus(AbstractClipJob::PROXYJOB, JobDone);
            bin->gotProxy(id, path);
            continue;
        }
        QString sourcePath = item->url();
        if (item->clipType() == Playlist) {
            // Special case: playlists use the special 'consumer' producer to support resizing
            sourcePath.prepend(QStringLiteral("consumer:"));
        }
        QStringList parameters;
        QTemporaryFile *playlist = nullptr;
        if (item->clipType() == SlideShow) {
            // we save a temporary .mlt clip for rendering
            QDomDocument doc;
            //TODO FIXME what we will do with xml ?
            QDomElement xml = item->toXml(doc, false);
            playlist = new QTemporaryFile();
            playlist->setFileTemplate(playlist->fileTemplate() + QStringLiteral(".mlt"));
            if (playlist->open()) {
                sourcePath = playlist->fileName();
                QTextStream out(playlist);
                out << doc.toString();
                playlist->close();
            }
        }
        qCDebug(KDENLIVE_LOG)<<" * *PROXY PATH: "<<path<<", "<<sourcePath;
        parameters << path << sourcePath << item->getProducerProperty(QStringLiteral("_exif_orientation")) << params << QString::number(renderSize.width()) << QString::number(renderSize.height());
        ProxyJob *job = new ProxyJob(item->clipType(), id, parameters, playlist);
        jobs.insert(item, job);
    }
    return jobs;
}

