/*
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "customjobtask.h"
#include "bin/bin.h"
#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/clipjobmanager.h"
#include "doc/kdenlivedoc.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "mainwindow.h"

#include <QProcess>
#include <QTemporaryFile>
#include <QThread>

#include <KLocalizedString>

CustomJobTask::CustomJobTask(const ObjectId &owner, const QString &jobName, const QStringList &jobParams, int in, int out, const QString &jobId,
                             QObject *object)
    : AbstractTask(owner, AbstractTask::TRANSCODEJOB, object)
    , m_jobDuration(0)
    , m_isFfmpegJob(true)
    , m_parameters(jobParams)
    , m_inPoint(in)
    , m_outPoint(out)
    , m_jobId(jobId)
    , m_jobProcess(nullptr)
{
    m_description = jobName;
}

void CustomJobTask::start(QObject *object, const QString &jobId)
{
    std::vector<QString> binIds = pCore->bin()->selectedClipsIds(true);
    QStringList jobData = ClipJobManager::getJobParameters(jobId);
    if (jobData.size() < 4) {
        qDebug() << ":::: INVALID JOB DATA FOR: " << jobId << "\n____________________";
        return;
    }
    const QString jobName = jobData.takeFirst();

    for (auto &id : binIds) {
        CustomJobTask *task = nullptr;
        ObjectId owner;
        int in = -1;
        int out = -1;
        if (id.contains(QLatin1Char('/'))) {
            QStringList binData = id.split(QLatin1Char('/'));
            if (binData.size() < 3) {
                // Invalid subclip data
                qDebug() << "=== INVALID SUBCLIP DATA: " << id;
                continue;
            }
            owner = {ObjectType::BinClip, binData.first().toInt(), QUuid()};
            in = binData.at(1).toInt();
            out = binData.at(2).toInt();
        } else {
            // Process full clip
            owner = {ObjectType::BinClip, id.toInt(), QUuid()};
        }
        task = new CustomJobTask(owner, jobName, jobData, in, out, jobId, object);
        if (task) {
            // Otherwise, start a filter thread.
            pCore->taskManager.startTask(owner.itemId, task);
        }
    }
}

void CustomJobTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        return;
    }
    QMutexLocker lock(&m_runMutex);
    m_running = true;
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
    if (!binClip) {
        return;
    }
    QString source = binClip->url();
    QString folderId = binClip->parent()->clipId();
    const QString binary = m_parameters.takeFirst();
    if (!QFile::exists(binary)) {
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                  Q_ARG(QString, i18n("Application %1 not found, please update the job settings", binary)),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        return;
    }
    m_isFfmpegJob = QFileInfo(binary).fileName().contains(QLatin1String("ffmpeg"));
    QString jobParameters = m_parameters.takeFirst();
    QStringList parameters;
    m_jobDuration = int(binClip->duration().seconds());

    // Tell ffmpeg to overwrite, we do the file exist check ourselves
    if (m_isFfmpegJob) {
        parameters << QStringLiteral("-y");
        if (m_inPoint > -1) {
            parameters << QStringLiteral("-ss") << QString::number(GenTime(m_inPoint, pCore->getCurrentFps()).seconds());
        }
        parameters << QStringLiteral("-stats");
    }
    parameters << jobParameters.split(QLatin1Char(' '));
    int inputIndex = parameters.indexOf(QStringLiteral("%1"));
    if (inputIndex > -1) {
        parameters.replace(inputIndex, source);
        if (m_isFfmpegJob && m_outPoint > -1) {
            parameters.insert(inputIndex + 1, QStringLiteral("-to"));
            parameters.insert(inputIndex + 2, QString::number(GenTime(m_outPoint - m_inPoint, pCore->getCurrentFps()).seconds()));
        }
    }
    if (m_isFfmpegJob) {
        // Only output error data
        parameters << QStringLiteral("-v") << QStringLiteral("error");
    }
    // Make sure we keep the stream order
    // parameters << QStringLiteral("-sn") << QStringLiteral("-dn") << QStringLiteral("-map") << QStringLiteral("0");
    QFileInfo sourceInfo(source);
    QString extension = m_parameters.takeFirst();
    if (extension.isEmpty()) {
        extension = sourceInfo.suffix();
    }
    if (!extension.startsWith(QLatin1Char('.'))) {
        extension.prepend(QLatin1Char('.'));
    }
    const QString destName = sourceInfo.baseName();
    QDir baseDir = sourceInfo.absoluteDir();
    QString destPath = baseDir.absoluteFilePath(destName + extension);
    if (QFileInfo::exists(destPath)) {
        QString fixedName = destName;
        static const QRegularExpression regex(QRegularExpression::anchoredPattern(QStringLiteral(R"(.*-(\d{4})$)")));
        QRegularExpressionMatch match = regex.match(fixedName);
        if (match.hasMatch()) {
            // if the file name has already an index suffix,
            // increase the number
            const int currentSuffix = match.captured(1).toInt();
            fixedName.replace(match.capturedStart(1), match.capturedLength(1), QString::asprintf("-%04d", currentSuffix + 1));
        } else {
            // if the file has no index suffix, append -0001
            fixedName.append(QString::asprintf("-%04d", 1));
        }
        destPath = baseDir.absoluteFilePath(fixedName + extension);
    }
    parameters << destPath;
    qDebug() << "/// FULL TRANSCODE PARAMS:\n" << parameters << "\n------";
    m_jobProcess.reset(new QProcess);
    // m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(this, &CustomJobTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &CustomJobTask::processLogInfo);
    m_jobProcess->start(binary, parameters, QIODevice::ReadOnly);
    AbstractTask::setPreferredPriority(m_jobProcess->processId());
    m_jobProcess->waitForFinished(-1);
    bool result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    // remove temporary playlist if it exists
    m_progress = 100;
    QMetaObject::invokeMethod(m_object, "updateJobProgress");
    if (result) {
        if (QFileInfo(destPath).size() == 0) {
            QFile::remove(destPath);
            // File was not created
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to create file.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
        } else {
            QMetaObject::invokeMethod(pCore->bin(), "addProjectClipInFolder", Qt::QueuedConnection, Q_ARG(QString, destPath),
                                      Q_ARG(QString, QString::number(m_owner.itemId)), Q_ARG(QString, folderId), Q_ARG(QString, m_jobId));
        }
    } else {
        // Proxy process crashed
        QFile::remove(destPath);
        if (!m_isCanceled) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to create file.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
        }
    }
}

void CustomJobTask::processLogInfo()
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
            // emit jobProgress(int(100.0 * progress / m_jobDuration));
        }
    } else {
        // Parse MLT output
        if (buffer.contains(QLatin1String("percentage:"))) {
            m_progress = buffer.section(QStringLiteral("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
            QMetaObject::invokeMethod(m_object, "updateJobProgress");
        }
    }
}
