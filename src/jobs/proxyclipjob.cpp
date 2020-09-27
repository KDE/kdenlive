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
#include <QThread>

#include <klocalizedstring.h>

ProxyJob::ProxyJob(const QString &binId)
    : AbstractClipJob(PROXYJOB, binId)
    , m_jobDuration(0)
    , m_isFfmpegJob(true)
    , m_jobProcess(nullptr)
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
    QFileInfo fInfo(dest);
    if (binClip->getProducerIntProperty(QStringLiteral("_overwriteproxy")) == 0 && fInfo.exists() && fInfo.size() > 0) {
        // Proxy clip already created
        m_done = true;
        return true;
    }
    ClipType::ProducerType type = binClip->clipType();
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
        if (parameter.isEmpty()) {
            // Automatic setting, decide based on hw support
            parameter = pCore->currentDoc()->getAutoProxyProfile();
            bool nvenc = parameter.contains(QStringLiteral("%nvcodec"));
            if (nvenc) {
                parameter = parameter.section(QStringLiteral("-i"), 1);
                parameter.replace(QStringLiteral("scale_cuda"), QStringLiteral("scale"));
                parameter.replace(QStringLiteral("scale_npp"), QStringLiteral("scale"));
                parameter.prepend(QStringLiteral("-pix_fmt yuv420p"));
            }
        }
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        QStringList params = parameter.split(QLatin1Char('-'), QString::SkipEmptyParts);
#else
        QStringList params = parameter.split(QLatin1Char('-'), Qt::SkipEmptyParts);
#endif
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
        for (const QString &s : qAsConst(params)) {
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
        int threadCount = QThread::idealThreadCount();
        if (threadCount > 2) {
            threadCount = qMin(threadCount - 1, 4);
        } else {
            threadCount = 1;
        }
        mltParameters.append(QStringLiteral("real_time=-%1").arg(threadCount));
        mltParameters.append(QStringLiteral("threads=%1").arg(threadCount));
        mltParameters.append(QStringLiteral("terminate_on_pause=1"));

        // TODO: currently, when rendering an xml file through melt, the display ration is lost, so we enforce it manually
        mltParameters << QStringLiteral("aspect=") + QString::number(display_ratio, 'f');

        // Ask for progress reporting
        mltParameters << QStringLiteral("progress=1");

        m_jobProcess = new QProcess;
        // m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(this, &ProxyJob::jobCanceled, m_jobProcess, &QProcess::kill, Qt::DirectConnection);
        connect(m_jobProcess, &QProcess::readyReadStandardError, this, &ProxyJob::processLogInfo);
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
        if (KdenliveSettings::ffmpegpath().isEmpty()) {
            // FFmpeg not detected, cannot process the Job
            m_errorMessage.prepend(i18n("Failed to create proxy. FFmpeg not found, please set path in Kdenlive's settings Environment"));
            m_done = true;
            return false;
        }
        // Only output error data, make sure we don't block when proxy file already exists
        QStringList parameters = {QStringLiteral("-hide_banner"), QStringLiteral("-y"), QStringLiteral("-stats"), QStringLiteral("-v"), QStringLiteral("error")};
        m_jobDuration = (int)binClip->duration().seconds();
        QString proxyParams = pCore->currentDoc()->getDocumentProperty(QStringLiteral("proxyparams")).simplified();
        if (proxyParams.isEmpty()) {
            // Automatic setting, decide based on hw support
            proxyParams = pCore->currentDoc()->getAutoProxyProfile();
        }
        bool nvenc = proxyParams.contains(QStringLiteral("%nvcodec"));
        if (nvenc) {
            QString pix_fmt = binClip->videoCodecProperty(QStringLiteral("pix_fmt"));
            QString codec = binClip->videoCodecProperty(QStringLiteral("name"));
            QStringList supportedCodecs{QStringLiteral("hevc"),  QStringLiteral("h264"),  QStringLiteral("mjpeg"),
                                        QStringLiteral("mpeg1"), QStringLiteral("mpeg2"), QStringLiteral("mpeg4"),
                                        QStringLiteral("vc1"),   QStringLiteral("vp8"),   QStringLiteral("vp9")};
            QStringList supportedPixFmts{QStringLiteral("yuv420p"), QStringLiteral("yuyv422"), QStringLiteral("rgb24"),
                                         QStringLiteral("bgr24"),   QStringLiteral("yuv422p"), QStringLiteral("yuv444p"),
                                         QStringLiteral("rgb32"),   QStringLiteral("yuv410p"), QStringLiteral("yuv411p")};
            bool supported = KdenliveSettings::nvScalingEnabled() && supportedCodecs.contains(codec) && supportedPixFmts.contains(pix_fmt);
            if (supported) {
                // Full hardware decoding supported
                codec.append(QStringLiteral("_cuvid"));
                proxyParams.replace(QStringLiteral("%nvcodec"), codec);
            } else {
                proxyParams = proxyParams.section(QStringLiteral("-i"), 1);
                proxyParams.replace(QStringLiteral("scale_cuda"), QStringLiteral("scale"));
                proxyParams.replace(QStringLiteral("scale_npp"), QStringLiteral("scale"));
                if (!supportedPixFmts.contains(pix_fmt)) {
                    proxyParams.prepend(QStringLiteral("-pix_fmt yuv420p"));
                }
            }
        }
        bool disableAutorotate = binClip->getProducerProperty(QStringLiteral("autorotate")) == QLatin1String("0");
        if (disableAutorotate || proxyParams.contains(QStringLiteral("-noautorotate"))) {
            // The noautorotate flag must be passed before input source
            parameters << QStringLiteral("-noautorotate");
        }
        if (proxyParams.contains(QLatin1String("-i "))) {
            // we have some pre-filename parameters, filename will be inserted later
        } else {
            parameters << QStringLiteral("-i") << source;
        }
        QString params = proxyParams;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        for (const QString &s : params.split(QLatin1Char(' '), QString::SkipEmptyParts)) {
#else
        for (const QString &s : params.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
#endif
            QString t = s.simplified();
            if (t != QLatin1String("-noautorotate")) {
                parameters << t;
                if (t == QLatin1String("-i")) {
                    parameters << source;
                }
            }
        }

        // Make sure we keep the stream order
        parameters << QStringLiteral("-sn") << QStringLiteral("-dn") << QStringLiteral("-map") << QStringLiteral("0");
        parameters << dest;
        qDebug()<<"/// FULL PROXY PARAMS:\n"<<parameters<<"\n------";
        m_jobProcess = new QProcess;
        // m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_jobProcess, &QProcess::readyReadStandardError, this, &ProxyJob::processLogInfo);
        connect(this, &ProxyJob::jobCanceled, m_jobProcess, &QProcess::kill, Qt::DirectConnection);
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters, QIODevice::ReadOnly);
        m_jobProcess->waitForFinished(-1);
        result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    }
    // remove temporary playlist if it exists
    if (result) {
        if (QFileInfo(dest).size() == 0) {
            QFile::remove(dest);
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
    m_jobProcess->deleteLater();
    return result;
}

void ProxyJob::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardError());
    m_logDetails.append(buffer);
    int progress = 0;
    if (m_isFfmpegJob) {
        // Parse FFmpeg output
        if (m_jobDuration == 0) {
            if (buffer.contains(QLatin1String("Duration:"))) {
                QString data = buffer.section(QStringLiteral("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
                if (!data.isEmpty()) {
                    QStringList numbers = data.split(QLatin1Char(':'));
                    if (numbers.size() < 3) {
                        return;
                    }
                    m_jobDuration = (int)(numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble());
                }
            }
        } else if (buffer.contains(QLatin1String("time="))) {
            QString time = buffer.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
            if (!time.isEmpty()) {
                QStringList numbers = time.split(QLatin1Char(':'));
                if (numbers.size() < 3) {
                    progress = (int)time.toDouble();
                    if (progress == 0) {
                        return;
                    }
                } else {
                    progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
                }
            }
            emit jobProgress((int)(100.0 * progress / m_jobDuration));
        }
    } else {
        // Parse MLT output
        if (buffer.contains(QLatin1String("percentage:"))) {
            progress = buffer.section(QStringLiteral("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
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
    auto operation = [clipId = m_clipId]() {
        auto binClip = pCore->projectItemModel()->getClipByBinID(clipId);
        binClip->setProducerProperty(QStringLiteral("_overwriteproxy"), QString());
        const QString dest = binClip->getProducerProperty(QStringLiteral("kdenlive:proxy"));
        binClip->setProducerProperty(QStringLiteral("resource"), dest);
        pCore->bin()->reloadClip(clipId);
        return true;
    };
    auto reverse = [clipId = m_clipId]() {
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
