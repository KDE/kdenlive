/*
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "proxytask.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"

#include <QImageReader>
#include <QProcess>
#include <QTemporaryFile>
#include <QThread>

#include <KLocalizedString>

ProxyTask::ProxyTask(const ObjectId &owner, QObject *object)
    : AbstractTask(owner, AbstractTask::PROXYJOB, object)
    , m_jobDuration(0)
    , m_isFfmpegJob(true)
{
    m_description = i18n("Creating proxy");
}

void ProxyTask::start(const ObjectId &owner, QObject *object, bool force)
{
    // See if there is already a task for this MLT service and resource.
    if (pCore->taskManager.hasPendingJob(owner, AbstractTask::PROXYJOB)) {
        return;
    }
    ProxyTask *task = new ProxyTask(owner, object);
    // Otherwise, start a new proxy generation thread.
    task->m_isForce = force;
    pCore->taskManager.startTask(owner.itemId, task);
}

void ProxyTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        return;
    }
    QMutexLocker lock(&m_runMutex);
    m_progress = 0;
    m_running = true;
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
    if (binClip == nullptr) {
        return;
    }
    const QString dest = binClip->getProducerProperty(QStringLiteral("kdenlive:proxy"));
    QFileInfo fInfo(dest);
    if (binClip->getProducerIntProperty(QStringLiteral("_overwriteproxy")) == 0 && fInfo.exists() && fInfo.size() > 0) {
        // Proxy clip already created
        m_progress = 100;
        QMetaObject::invokeMethod(m_object, "updateJobProgress");
        QMetaObject::invokeMethod(binClip.get(), "updateProxyProducer", Qt::QueuedConnection, Q_ARG(QString, dest));
        return;
    }

    ClipType::ProducerType type = binClip->clipType();
    m_progress = 0;
    bool result = false;
    QString source = binClip->getProducerProperty(QStringLiteral("kdenlive:originalurl"));
    bool disable_exif = binClip->getProducerIntProperty(QStringLiteral("disable_exif")) == 1;

    if (type == ClipType::Playlist || type == ClipType::SlideShow) {
        // change FFmpeg params to MLT format
        m_isFfmpegJob = false;
        QStringList mltParameters = {QStringLiteral("-loglevel"), QStringLiteral("error")};
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
        mltParameters << QStringLiteral("-profile") << pCore->getCurrentProfilePath();
        mltParameters << source;
        // set destination
        mltParameters << QStringLiteral("-consumer") << QStringLiteral("avformat:%1").arg(dest) << QStringLiteral("out=%1").arg(binClip->frameDuration());
        QString parameter;
        if (binClip->hasAlpha()) {
            // check if this is a VP8/VP9 clip and enforce libvpx codec
            parameter = KdenliveSettings::proxyalphaparams().simplified();
            if (parameter.isEmpty()) {
                // Automatic setting, decide based on hw support
                parameter = pCore->currentDoc()->getAutoProxyAlphaProfile();
            }
        } else {
            parameter = pCore->currentDoc()->getDocumentProperty(QStringLiteral("proxyparams")).simplified();
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
        }
        int proxyResize = pCore->currentDoc()->getDocumentProperty(QStringLiteral("proxyresize")).toInt();
        parameter.replace(QStringLiteral("%width"), QString::number(proxyResize));
        if (parameter.contains(QLatin1String("-i "))) {
            // Remove the source input if any
            parameter.remove(QLatin1String("-i "));
        }

        QStringList params = parameter.split(QLatin1Char('-'), Qt::SkipEmptyParts);
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
        for (const QString &s : std::as_const(params)) {
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
                int height = int(width / display_ratio);
                // Make sure we get an even height
                height += height % 2;
                mltParameters << QStringLiteral("s=%1x%2").arg(width).arg(height);
                if (t.contains(QStringLiteral("yadif"))) {
                    mltParameters << QStringLiteral("progressive=1");
                }
                continue;
            } else {
                t.replace(QLatin1Char(' '), QLatin1String("="));
                if (t == QLatin1String("acodec=copy") && type == ClipType::Playlist) {
                    // drop this for playlists, otherwise we have no sound in proxies
                    continue;
                }
            }
            mltParameters << t;
        }
        int threadCount = QThread::idealThreadCount();
        if (threadCount > 2) {
            threadCount = qMin(threadCount - 1, 4);
        } else {
            threadCount = 1;
        }
        // real_time parameter seems to cause rendering artifacts with playlist clips
        // mltParameters.append(QStringLiteral("real_time=-%1").arg(threadCount));
        mltParameters.append(QStringLiteral("threads=%1").arg(threadCount));
        mltParameters.append(QStringLiteral("terminate_on_pause=1"));

        // TODO: currently, when rendering an xml file through melt, the display ration is lost, so we enforce it manually
        mltParameters << QStringLiteral("aspect=") + QString::number(display_ratio, 'f');

        // Ask for progress reporting
        mltParameters << QStringLiteral("progress=1");

        m_jobProcess = new QProcess();
        // m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        qDebug() << " :: STARTING PLAYLIST PROXY: " << mltParameters;
        QObject::connect(this, &ProxyTask::jobCanceled, m_jobProcess, &QProcess::kill, Qt::DirectConnection);
        QObject::connect(m_jobProcess, &QProcess::readyReadStandardError, this, &ProxyTask::processLogInfo);
        m_jobProcess->start(KdenliveSettings::meltpath(), mltParameters);
        AbstractTask::setPreferredPriority(m_jobProcess->processId());
        m_jobProcess->waitForFinished(-1);
        result = m_jobProcess->exitStatus() == QProcess::NormalExit;
        m_jobProcess->deleteLater();
        delete playlist;
    } else if (type == ClipType::Image) {
        m_isFfmpegJob = false;
        // Image proxy
        QImageReader imgReader(source);
        imgReader.setAutoTransform(!disable_exif);
        QImage i = imgReader.read();
        if (i.isNull()) {
            result = false;
            QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Cannot load image %1.", source)),
                                      Q_ARG(int, int(KMessageWidget::Warning)));
            m_progress = 100;
            QMetaObject::invokeMethod(m_object, "updateJobProgress");
            return;
        }

        QImage proxy;
        // Images are scaled to predefined size.
        int maxSize = qMax(i.width(), i.height());
        if (KdenliveSettings::proxyimagesize() < maxSize) {
            if (i.width() > i.height()) {
                proxy = i.scaledToWidth(KdenliveSettings::proxyimagesize());
            } else {
                proxy = i.scaledToHeight(KdenliveSettings::proxyimagesize());
            }
            proxy.save(dest);
            result = true;
        } else {
            // Image is too small to be proxied
            m_logDetails = i18n("Image too small to be proxied.");
            result = false;
        }
    } else {
        m_isFfmpegJob = true;
        if (!QFileInfo(KdenliveSettings::ffmpegpath()).isFile()) {
            // FFmpeg not detected, cannot process the Job
            QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                      Q_ARG(QString, i18n("FFmpeg not found, please set path in Kdenlive's settings Environment")),
                                      Q_ARG(int, int(KMessageWidget::Warning)));
            result = true;
            m_progress = 100;
            QMetaObject::invokeMethod(m_object, "updateJobProgress");
            return;
        }
        // Only output error data, make sure we don't block when proxy file already exists
        QStringList parameters = {QStringLiteral("-hide_banner"), QStringLiteral("-y"), QStringLiteral("-stats"), QStringLiteral("-v"),
                                  QStringLiteral("error")};
        m_jobDuration = int(binClip->duration().seconds());
        if (binClip->hasProducerProperty(QStringLiteral("kdenlive:camcorderproxy"))) {
            // ffmpeg -an -i proxy.mp4 -vn -i original.MXF -map 0:v -map 1:a -c:v copy out.MP4
            // Create a new proxy file with video from camcorder proxy and audio from source clip
            const QString proxyPath = binClip->getProducerProperty(QStringLiteral("kdenlive:camcorderproxy"));
            parameters << QStringLiteral("-an") << QStringLiteral("-i") << proxyPath;
            parameters << QStringLiteral("-vn") << QStringLiteral("-i") << source;
            parameters << QStringLiteral("-map") << QStringLiteral("0:v") << QStringLiteral("-map") << QStringLiteral("1:a");
            parameters << QStringLiteral("-c:v") << QStringLiteral("copy") << dest;
        } else {
            QString proxyParams;
            if (binClip->hasAlpha()) {
                // check if this is a VP8/VP9 clip and enforce libvpx codec
                proxyParams = KdenliveSettings::proxyalphaparams().simplified();
                if (proxyParams.isEmpty()) {
                    // Automatic setting, decide based on hw support
                    proxyParams = pCore->currentDoc()->getAutoProxyAlphaProfile();
                }
                if ((binClip->clipType() == ClipType::AV || binClip->clipType() == ClipType::Video)) {
                    const QString codec = binClip->videoCodecProperty(QStringLiteral("name"));
                    if (codec == QLatin1String("libvpx-vp9")) {
                        proxyParams.prepend(QStringLiteral("-c:v libvpx-vp9 -i "));
                    } else if (codec == QLatin1String("libvpx")) {
                        proxyParams.prepend(QStringLiteral("-c:v libvpx -i "));
                    }
                }
            } else {
                proxyParams = pCore->currentDoc()->getDocumentProperty(QStringLiteral("proxyparams")).simplified();
                if (proxyParams.isEmpty()) {
                    // Automatic setting, decide based on hw support
                    proxyParams = pCore->currentDoc()->getAutoProxyProfile();
                }
            }
            int proxyResize = pCore->currentDoc()->getDocumentProperty(QStringLiteral("proxyresize")).toInt();
            if (!proxyParams.contains(QLatin1String("mjpeg")) && !proxyParams.contains(QLatin1String("mpeg2video"))) {
                parameters << QStringLiteral("-noautorotate");
            }
            // Check if source is interlaced
            bool interlacedSource = false;
            // TODO: should proxy clips keep interlacing ?
            /*if (binClip->getProducerIntProperty(QStringLiteral("meta.media.progressive")) == 0) {
                // Interlaced
                interlacedSource = true;
                qDebug() << "::: Interlaced content disabling nvidia codecs";
            } else {
                qDebug() << "::: Found progressive content";
            }*/
            bool vaapi = proxyParams.contains(QStringLiteral("vaapi"));
            if (vaapi && interlacedSource) {
                // Disable vaapi if source is interlaced
                proxyParams = proxyParams.section(QStringLiteral("-i "), 1);
                proxyParams.replace(QStringLiteral(",format=nv12,hwupload"), QString());
                proxyParams.replace(QStringLiteral("h264_vaapi"), QStringLiteral("libx264"));
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

                // Check if the transcoded file uses a cuda supported codec (we don't check for specific cards so not 100% exact)
                bool supported = !interlacedSource && supportedCodecs.contains(codec) && supportedPixFmts.contains(pix_fmt);
                if (supported && proxyParams.contains(QStringLiteral("scale_npp")) && !KdenliveSettings::nvScalingEnabled()) {
                    supported = false;
                }
                if (supported && proxyParams.contains(QStringLiteral("%frameSize"))) {
                    int w = proxyResize;
                    int h = 0;
                    int oW = binClip->getProducerProperty(QStringLiteral("meta.media.width")).toInt();
                    int oH = binClip->getProducerProperty(QStringLiteral("meta.media.height")).toInt();
                    if (oH > 0) {
                        h = w * oH / oW;
                    } else {
                        h = int(w / pCore->getCurrentDar());
                    }
                    h += h % 2;
                    proxyParams.replace(QStringLiteral("%frameSize"), QStringLiteral("%1x%2").arg(w).arg(h));
                }
                if (supported) {
                    // Full hardware decoding supported
                    codec.append(QStringLiteral("_cuvid"));
                    proxyParams.replace(QStringLiteral("%nvcodec"), codec);
                } else {
                    proxyParams = proxyParams.section(QStringLiteral("-i "), 1);
                    if (!supportedPixFmts.contains(pix_fmt)) {
                        proxyParams.prepend(QStringLiteral("-pix_fmt yuv420p "));
                    }
                    proxyParams.replace(QStringLiteral("scale_cuda"), QStringLiteral("scale"));
                    proxyParams.replace(QStringLiteral("scale_npp"), QStringLiteral("scale"));
                    if (interlacedSource) {
                        // nVidia does not support interlaced encoding
                        proxyParams.replace(QStringLiteral("h264_nvenc"), QStringLiteral("libx264"));
                    }
                }
            }
            proxyParams.replace(QStringLiteral("%width"), QString::number(proxyResize));
            bool disableAutorotate = binClip->getProducerProperty(QStringLiteral("autorotate")) == QLatin1String("0");
            if (disableAutorotate || proxyParams.contains(QStringLiteral("-noautorotate"))) {
                // The noautorotate flag must be passed before input source
                parameters << QStringLiteral("-noautorotate");
                proxyParams.replace(QStringLiteral("-noautorotate"), QString());
            }
            if (proxyParams.contains(QLatin1String("-i "))) {
                // we have some pre-filename parameters, filename will be inserted later
            } else {
                parameters << QStringLiteral("-i") << source;
            }
            const QStringList params = proxyParams.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &s : params) {
                QString t = s.simplified();
                parameters << t;
                if (t == QLatin1String("-i")) {
                    parameters << source;
                }
            }
            if (interlacedSource) {
                // Keep interlacing
                parameters << QStringLiteral("-flags") << QStringLiteral("+ildct+ilme") << QStringLiteral("-top") << QStringLiteral("-1");
            }

            // Make sure we keep the stream order
            parameters << QStringLiteral("-sn") << QStringLiteral("-dn");
            if (binClip->hasProducerProperty(QStringLiteral("kdenlive:coverartstream"))) {
                // int streamIx = binClip->getProducerIntProperty(QStringLiteral("kdenlive:coverartstream"));
                // Use 0:V to drop cover art streams
                // TODO: this might change the streams index
                parameters << QStringLiteral("-map") << QStringLiteral("0:V");
                parameters << QStringLiteral("-map") << QStringLiteral("0:a");
            } else {
                parameters << QStringLiteral("-map") << QStringLiteral("0");
            }
            // Drop unknown streams instead of aborting
            parameters << QStringLiteral("-ignore_unknown");
            parameters << dest;
            qDebug() << "/// FULL PROXY PARAMS:\n" << parameters << "\n------";
        }
        m_jobProcess = new QProcess(this);
        // m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(m_jobProcess, &QProcess::readyReadStandardError, this, &ProxyTask::processLogInfo);
        QObject::connect(this, &ProxyTask::jobCanceled, m_jobProcess, &QProcess::kill, Qt::DirectConnection);
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters, QIODevice::ReadOnly);
        AbstractTask::setPreferredPriority(m_jobProcess->processId());
        m_jobProcess->waitForFinished(-1);
        result = m_jobProcess->exitStatus() == QProcess::NormalExit && m_jobProcess->exitCode() == 0;
        m_jobProcess->deleteLater();
    }
    // remove temporary playlist if it exists
    m_progress = 100;
    if (result && !m_isCanceled) {
        if (QFileInfo(dest).size() == 0) {
            QFile::remove(dest);
            // File was not created
            result = false;
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to create proxy clip.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
            if (binClip) {
                binClip->setProducerProperty(QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
            }
        } else if (binClip) {
            // Job successful
            QMetaObject::invokeMethod(binClip.get(), "updateProxyProducer", Qt::QueuedConnection, Q_ARG(QString, dest));
        }
    } else {
        // Proxy process crashed of failed
        QFile::remove(dest);
        if (!m_isCanceled) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to create proxy clip.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
        }
    }
    QMetaObject::invokeMethod(m_object, "updateJobProgress");
    return;
}

void ProxyTask::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardError());
    m_logDetails.append(buffer);
    if (m_isFfmpegJob) {
        // Parse FFmpeg output
        if (m_jobDuration == 0) {
            if (buffer.contains(QLatin1String("Duration:"))) {
                const QString data = buffer.section(QStringLiteral("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
                if (!data.isEmpty()) {
                    QStringList numbers = data.split(QLatin1Char(':'));
                    if (numbers.size() < 3) {
                        return;
                    }
                    m_jobDuration = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toInt();
                }
            }
        } else if (buffer.contains(QLatin1String("time="))) {
            int progress = 0;
            const QString time = buffer.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
            if (!time.isEmpty()) {
                const QStringList numbers = time.split(QLatin1Char(':'));
                if (numbers.size() < 3) {
                    progress = time.toInt();
                    if (progress == 0) {
                        return;
                    }
                } else {
                    progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + qRound(numbers.at(2).toDouble());
                }
            }
            int val = 100 * progress / m_jobDuration;
            if (m_progress != val) {
                m_progress = val;
                QMetaObject::invokeMethod(m_object, "updateJobProgress");
            }
            // Q_EMIT jobProgress(int(100.0 * progress / m_jobDuration));
        }
    } else {
        // Parse MLT output
        if (buffer.contains(QLatin1String("percentage:"))) {
            int val = buffer.section(QStringLiteral("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
            if (m_progress != val) {
                m_progress = val;
                QMetaObject::invokeMethod(m_object, "updateJobProgress");
            }
            // Q_EMIT jobProgress(progress);
        }
    }
}
