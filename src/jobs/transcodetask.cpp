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

#include "transcodetask.h"
#include "bin/bin.h"
#include "mainwindow.h"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "bin/clipcreator.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"

#include <QProcess>
#include <QTemporaryFile>
#include <QThread>

#include <klocalizedstring.h>

TranscodeTask::TranscodeTask(const ObjectId &owner, QString params, int in, int out, bool replaceProducer, QObject* object)
    : AbstractTask(owner, AbstractTask::TRANSCODEJOB, object)
    , m_jobDuration(0)
    , m_isFfmpegJob(true)
    , m_transcodeParams(params)
    , m_replaceProducer(replaceProducer)
    , m_inPoint(in)
    , m_outPoint(out)
    , m_jobProcess(nullptr)
{
}

void TranscodeTask::start(const ObjectId &owner, QString params, int in, int out, bool replaceProducer, QObject* object, bool force)
{
    TranscodeTask* task = new TranscodeTask(owner, params, in, out, replaceProducer, object);
    // See if there is already a task for this MLT service and resource.
    if (pCore->taskManager.hasPendingJob(owner, AbstractTask::TRANSCODEJOB)) {
        delete task;
        task = 0;
    }
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
    const QString source = binClip->url();
    ClipType::ProducerType type = binClip->clipType();
    QString transcoderExt = m_transcodeParams.section(QLatin1String("%1"), 1).section(QLatin1Char(' '), 0, 0);
    if (transcoderExt.isEmpty()) {
        qDebug()<<"// INVALID TRANSCODING PROFILE";
        m_progress = 100;
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    QFileInfo finfo(source);
    QString fileName = finfo.fileName().section(QLatin1Char('.'), 0, -2);
    QDir dir = finfo.absoluteDir();
    int fileCount = 1;
    QString num = QString::number(fileCount).rightJustified(4, '0', false);
    QString path = fileName + num + transcoderExt;
    while (dir.exists(path)) {
        ++fileCount;
        num = QString::number(fileCount).rightJustified(4, '0', false);
        path = fileName + num + transcoderExt;
    }
    QString destUrl = dir.absoluteFilePath(fileName);
    destUrl.append(QString::number(fileCount).rightJustified(4, '0', false));

    bool result;
    if (type == ClipType::Playlist || type == ClipType::SlideShow) {
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
        m_jobProcess->waitForFinished(-1);
        result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    } else {
        m_isFfmpegJob = true;
        QStringList parameters;
        if (KdenliveSettings::ffmpegpath().isEmpty()) {
            // FFmpeg not detected, cannot process the Job
            m_errorMessage.prepend(i18n("Failed to create proxy. FFmpeg not found, please set path in Kdenlive's settings Environment"));
            pCore->taskManager.taskDone(m_owner.second, this);
            return;
        }
        m_jobDuration = int(binClip->duration().seconds());
        parameters << QStringLiteral("-y");
        if (m_inPoint > -1) {
            parameters << QStringLiteral("-ss") << QString::number(GenTime(m_inPoint, pCore->getCurrentFps()).seconds());
        }
        parameters << QStringLiteral("-stats") << QStringLiteral("-i") << source;
        if (m_outPoint > -1) {
            parameters << QStringLiteral("-to") << QString::number(GenTime(m_outPoint - m_inPoint, pCore->getCurrentFps()).seconds());
        }
        // Only output error data
        parameters << QStringLiteral("-v") << QStringLiteral("error");
        QStringList params = m_transcodeParams.split(QLatin1Char(' '));
        QStringList finalParams{QStringLiteral("-i"),source};
        for (const QString &s : qAsConst(params)) {
            QString t = s.simplified();
            if (t.startsWith(QLatin1String("%1"))) {
                parameters << t.replace(QLatin1String("%1"), destUrl);
            } else {
                parameters << t;
            }
        }
        qDebug()<<"/// FULL PROXY PARAMS:\n"<<parameters<<"\n------";
        m_jobProcess.reset(new QProcess);
        // m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        QObject::connect(this, &TranscodeTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
        QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &TranscodeTask::processLogInfo);
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters, QIODevice::ReadOnly);
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
            m_errorMessage.append(i18n("Failed to create file."));
        } else {
            QString id = QString::number(m_owner.second);
            auto binClip = pCore->projectItemModel()->getClipByBinID(id);
            if (m_replaceProducer && binClip) {
                QMap <QString, QString> sourceProps;
                QMap <QString, QString> newProps;
                sourceProps.insert(QStringLiteral("resource"), binClip->url());
                sourceProps.insert(QStringLiteral("kdenlive:clipname"), binClip->clipName());
                newProps.insert(QStringLiteral("resource"), destUrl);
                newProps.insert(QStringLiteral("kdenlive:clipname"), QFileInfo(destUrl).fileName());
                pCore->bin()->slotEditClipCommand(id, sourceProps, newProps);
            } else {
                QString folder = QStringLiteral("-1");
                if (binClip) {
                    auto containingFolder = std::static_pointer_cast<ProjectFolder>(binClip->parent());
                    if (containingFolder) {
                        folder = containingFolder->clipId();
                    }
                }
                QMetaObject::invokeMethod(pCore->window(), "addProjectClip", Qt::QueuedConnection, Q_ARG(const QString&,destUrl), Q_ARG(const QString&,folder));
                //id = ClipCreator::createClipFromFile(destUrl, folderId, pCore->projectItemModel());
            }
        }
    } else {
        // Proxy process crashed
        QFile::remove(destUrl);;
        m_errorMessage.append(QString::fromUtf8(m_jobProcess->readAll()));
    }
}

void TranscodeTask::processLogInfo()
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
                    m_jobDuration = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toInt();
                }
            }
        } else if (buffer.contains(QLatin1String("time="))) {
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
            //emit jobProgress(progress);
        }
    }
}
