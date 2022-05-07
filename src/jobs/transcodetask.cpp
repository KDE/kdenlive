/*
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transcodetask.h"
#include "bin/bin.h"
#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "mainwindow.h"

#include <QProcess>
#include <QTemporaryFile>
#include <QThread>

#include <klocalizedstring.h>

TranscodeTask::TranscodeTask(const ObjectId &owner, const QString &suffix, const QString &preParams, const QString &params, int in, int out, bool replaceProducer, QObject* object, bool checkProfile)
    : AbstractTask(owner, AbstractTask::TRANSCODEJOB, object)
    , m_jobDuration(0)
    , m_isFfmpegJob(true)
    , m_suffix(suffix)
    , m_transcodeParams(params)
    , m_transcodePreParams(preParams)
    , m_replaceProducer(replaceProducer)
    , m_inPoint(in)
    , m_outPoint(out)
    , m_checkProfile(checkProfile)
    , m_jobProcess(nullptr)
{
}

void TranscodeTask::start(const ObjectId &owner, const QString &suffix, const QString &preParams, const QString &params, int in, int out, bool replaceProducer, QObject* object, bool force, bool checkProfile)
{
    // See if there is already a task for this MLT service and resource.
    if (pCore->taskManager.hasPendingJob(owner, AbstractTask::TRANSCODEJOB)) {
        return;
    }
    TranscodeTask* task = new TranscodeTask(owner, suffix, preParams, params, in, out, replaceProducer, object, checkProfile);
    if (task) {
        // Otherwise, start a new audio levels generation thread.
        task->m_isForce = force;
        pCore->taskManager.startTask(owner.second, task);
    }
}

void TranscodeTask::run()
{
    if (m_isCanceled) {
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    m_running = true;
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.second));
    ClipType::ProducerType type = binClip->clipType();
    QString source;
    QTemporaryFile src;
    if (type == ClipType::Text) {
        src.setFileTemplate(QDir::temp().absoluteFilePath(QString("XXXXXX.mlt")));
        if (src.open()) {
            source = src.fileName();
            QDomDocument doc;
            binClip->getProducerXML(doc, false, true);
            QTextStream out(&src);
            out.setCodec("UTF-8");
            out << doc.toString();
            src.close();
        }
    } else {
        source = binClip->url();
    }
    if (source.isEmpty()) {
        return;
    }

    QString transcoderExt = m_transcodeParams.section(QLatin1String("%1"), 1).section(QLatin1Char(' '), 0, 0);
    if (transcoderExt.isEmpty()) {
        qDebug()<<"// INVALID TRANSCODING PROFILE";
        m_progress = 100;
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    QFileInfo finfo(source);
    QString fileName;
    QDir dir;
    if (type == ClipType::Text) {
        fileName = binClip->name();
        dir = QDir(pCore->currentDoc()->url().isValid() ? pCore->currentDoc()->url().adjusted(QUrl::RemoveFilename).toLocalFile() : KdenliveSettings::defaultprojectfolder());
    } else {
        fileName = finfo.fileName().section(QLatin1Char('.'), 0, -2);
        dir = finfo.absoluteDir();
    }
    int fileCount = 1;
    QString num = QString::number(fileCount).rightJustified(4, '0', false);
    QString path;
    if (m_suffix.isEmpty()) {
        path = fileName + num + transcoderExt;
    } else {
        path = fileName + m_suffix + transcoderExt;
        fileCount = 0;
    }
    while (dir.exists(path)) {
        ++fileCount;
        num = QString::number(fileCount).rightJustified(4, '0', false);
        path = fileName + num + m_suffix + transcoderExt;
    }
    QString destUrl = dir.absoluteFilePath(path.section(QLatin1Char('.'), 0, -2));

    bool result;
    if (type == ClipType::Playlist || type == ClipType::SlideShow || type == ClipType::Text) {
        // change FFmpeg params to MLT format
        m_isFfmpegJob = false;
        // insert transcoded filename
        m_transcodeParams.replace(QStringLiteral("%1"), QString("-consumer %1"));
        // Convert param style
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        QStringList params = m_transcodeParams.split(QLatin1Char('-'), QString::SkipEmptyParts);
#else
        QStringList params = m_transcodeParams.split(QLatin1Char('-'), Qt::SkipEmptyParts);
#endif
        QStringList mltParameters;
        for (const QString &s : qAsConst(params)) {
            QString t = s.simplified();
            if (t.count(QLatin1Char(' ')) == 0) {
                t.append(QLatin1String("=1"));
            } else {
                if (t.contains(QLatin1String("%1"))) {
                    // file name
                    mltParameters.prepend(t.section(QLatin1Char(' '), 1).replace(QLatin1String("%1"), QString("avformat:%1").arg(destUrl)));
                    mltParameters.prepend(QStringLiteral("-consumer"));
                    continue;
                }
                if (t.startsWith(QLatin1String("aspect "))) {
                    // Fix aspect ratio calculation
                    t.replace(QLatin1Char(' '), QLatin1String("=@"));
                    t.replace(QLatin1Char(':'), QLatin1String("/"));
                } else {
                    t.replace(QLatin1Char(' '), QLatin1String("="));
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
        mltParameters.append(QStringLiteral("real_time=-%1").arg(threadCount));
        mltParameters.append(QStringLiteral("threads=%1").arg(threadCount));

        // Ask for progress reporting
        mltParameters << QStringLiteral("progress=1");
        if (m_outPoint > 0) {
            mltParameters.prepend(QString("out=%1").arg(m_outPoint));
            mltParameters.prepend(QString("in=%1").arg(m_inPoint));
        }
        mltParameters.prepend(source);
        m_jobProcess.reset(new QProcess);
        // m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(this, &TranscodeTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
        QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &TranscodeTask::processLogInfo);
        m_jobProcess->start(KdenliveSettings::rendererpath(), mltParameters);
        AbstractTask::setPreferredPriority(m_jobProcess->processId());
        m_jobProcess->waitForFinished(-1);
        result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    } else {
        m_isFfmpegJob = true;
        QStringList parameters;
        if (KdenliveSettings::ffmpegpath().isEmpty()) {
            // FFmpeg not detected, cannot process the Job
            QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("FFmpeg not found, please set path in Kdenlive's settings Environment")),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
            pCore->taskManager.taskDone(m_owner.second, this);
            return;
        }
        m_jobDuration = int(binClip->duration().seconds());
        parameters << QStringLiteral("-y");
        if (m_inPoint > -1) {
            parameters << QStringLiteral("-ss") << QString::number(GenTime(m_inPoint, pCore->getCurrentFps()).seconds());
        }
        parameters << QStringLiteral("-stats");
        if (!m_transcodePreParams.isEmpty()) {
            parameters << m_transcodePreParams.split(QStringLiteral(" "));
        }
        parameters << QStringLiteral("-i") << source;
        if (m_outPoint > -1) {
            parameters << QStringLiteral("-to") << QString::number(GenTime(m_outPoint - m_inPoint, pCore->getCurrentFps()).seconds());
        }
        // Only output error data
        parameters << QStringLiteral("-v") << QStringLiteral("error");
        // Make sure we keep the stream order
        parameters << QStringLiteral("-sn") << QStringLiteral("-dn") << QStringLiteral("-map") << QStringLiteral("0");
        QStringList params = m_transcodeParams.split(QLatin1Char(' '));
        for (const QString &s : qAsConst(params)) {
            QString t = s.simplified();
            if (t.startsWith(QLatin1String("%1"))) {
                parameters << t.replace(QLatin1String("%1"), destUrl);
            } else {
                parameters << t;
            }
        }
        qDebug()<<"/// FULL TRANSCODE PARAMS:\n"<<parameters<<"\n------";
        m_jobProcess.reset(new QProcess);
        // m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(this, &TranscodeTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
        QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &TranscodeTask::processLogInfo);
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters, QIODevice::ReadOnly);
        AbstractTask::setPreferredPriority(m_jobProcess->processId());
        m_jobProcess->waitForFinished(-1);
        result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    }
    destUrl.append(transcoderExt);
    // remove temporary playlist if it exists
    m_progress = 100;
    pCore->taskManager.taskDone(m_owner.second, this);
    QMetaObject::invokeMethod(m_object, "updateJobProgress");
    if (result) {
        if (QFileInfo(destUrl).size() == 0) {
            QFile::remove(destUrl);
            // File was not created
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to create file.")),
                                  Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
        } else {
            if (m_replaceProducer && binClip) {
                QMap <QString, QString> sourceProps;
                QMap <QString, QString> newProps;
                sourceProps.insert(QStringLiteral("resource"), binClip->url());
                sourceProps.insert(QStringLiteral("kdenlive:originalurl"), binClip->url());
                sourceProps.insert(QStringLiteral("kdenlive:proxy"), binClip->getProducerProperty(QStringLiteral("kdenlive:proxy")));
                sourceProps.insert(QStringLiteral("kdenlive:clipname"), binClip->clipName());
                sourceProps.insert(QStringLiteral("_fullreload"), QStringLiteral("1"));
                newProps.insert(QStringLiteral("resource"), destUrl);
                newProps.insert(QStringLiteral("kdenlive:originalurl"), destUrl);
                newProps.insert(QStringLiteral("kdenlive:clipname"), QFileInfo(destUrl).fileName());
                newProps.insert(QStringLiteral("kdenlive:proxy"), QString());
                newProps.insert(QStringLiteral("_fullreload"), QStringLiteral("1"));
                pCore->bin()->slotEditClipCommand(binClip->clipId(), sourceProps, newProps);
                if (m_checkProfile) {
                    QMetaObject::invokeMethod(pCore->bin(), "slotCheckProfile", Qt::QueuedConnection, Q_ARG(QString, QString::number(m_owner.second)));
                }
            } else {
                QString folder = QStringLiteral("-1");
                if (binClip) {
                    auto containingFolder = std::static_pointer_cast<ProjectFolder>(binClip->parent());
                    if (containingFolder) {
                        folder = containingFolder->clipId();
                    }
                }
                QMetaObject::invokeMethod(pCore->window(), "addProjectClip", Qt::QueuedConnection, Q_ARG(QString,destUrl), Q_ARG(QString,folder));
                //id = ClipCreator::createClipFromFile(destUrl, folderId, pCore->projectItemModel());
            }
        }
    } else {
        // Proxy process crashed
        QFile::remove(destUrl);
        if (!m_isCanceled) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to create file.")),
                                  Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
        }
    }
}

void TranscodeTask::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardError());
    m_logDetails.append(buffer);
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
                    m_jobDuration = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toInt();
                }
            }
        } else if (buffer.contains(QLatin1String("time="))) {
            int progress = 0;
            QString time = buffer.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
            if (!time.isEmpty()) {
                QStringList numbers = time.split(QLatin1Char(':'));
                if (numbers.size() < 3) {
                    progress = time.toInt();
                    if (progress == 0) {
                        return;
                    }
                } else {
                    progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + qRound(numbers.at(2).toDouble());
                }
            }
            m_progress = 100 * progress / m_jobDuration;
            QMetaObject::invokeMethod(m_object, "updateJobProgress");
            //emit jobProgress(int(100.0 * progress / m_jobDuration));
        }
    } else {
        // Parse MLT output
        if (buffer.contains(QLatin1String("percentage:"))) {
            m_progress = buffer.section(QStringLiteral("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
            QMetaObject::invokeMethod(m_object, "updateJobProgress");
        }
    }
}
