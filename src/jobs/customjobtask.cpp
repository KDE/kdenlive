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
#include "monitor/monitor.h"
#include "ui_customjobinterface_ui.h"

#include <QProcess>
#include <QTemporaryFile>
#include <QThread>

#include <KLocalizedString>

static QStringList requestedOutput;

CustomJobTask::CustomJobTask(const ObjectId &owner, const QString &jobName, const QMap<QString, QString> &jobParams, int in, int out, const QString &jobId,
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
    m_tmpFrameFile.setFileTemplate(QString("%1/kdenlive-frame-.XXXXXX.png").arg(QDir::tempPath()));
}

void CustomJobTask::start(QObject *object, const QString &jobId)
{
    std::vector<QString> binIds = pCore->bin()->selectedClipsIds(true);
    QMap<QString, QString> jobData = ClipJobManager::getJobParameters(jobId);
    if (jobData.size() < 4) {
        qDebug() << ":::: INVALID JOB DATA FOR: " << jobId << "\n____________________";
        return;
    }
    const QString jobName = jobData.value("description");
    QString param1Value;
    QString param2Value;
    const QString paramArgs = jobData.value("parameters");
    bool requestParam1 = paramArgs.contains(QLatin1String("{param1}"));
    bool requestParam2 = paramArgs.contains(QLatin1String("{param2}"));
    if (requestParam1 || requestParam2) {
        const QString param1 = jobData.value(QLatin1String("param1type"));
        const QString param2 = jobData.value(QLatin1String("param2type"));
        // We need to request user data for the job parameters
        QScopedPointer<QDialog> dia(new QDialog(QApplication::activeWindow()));
        Ui::CustomJobInterface_UI dia_ui;
        dia_ui.setupUi(dia.data());
        const QString jobDetails = jobData.value(QLatin1String("details"));
        if (!jobDetails.isEmpty()) {
            dia_ui.taskDescription->setPlainText(jobDetails);
        } else {
            dia_ui.taskDescription->setVisible(false);
        }
        dia->setWindowTitle(i18n("%1 parameters", jobName));
        if (requestParam1 && param1 != QLatin1String("frame")) {
            dia_ui.param1Label->setText(jobData.value(QLatin1String("param1name")));
            if (param1 == QLatin1String("file")) {
                dia_ui.param1List->setVisible(false);
            } else {
                dia_ui.param1Url->setVisible(false);
                QStringList listValues = jobData.value(QLatin1String("param1list")).split(QLatin1String("  "));
                dia_ui.param1List->addItems(listValues);
            }
        } else {
            // Hide param1
            dia_ui.param1Label->setVisible(false);
            dia_ui.param1Url->setVisible(false);
            dia_ui.param1List->setVisible(false);
        }
        if (requestParam2) {
            dia_ui.param2Label->setText(jobData.value(QLatin1String("param2name")));
            if (param2 == QLatin1String("file")) {
                dia_ui.param2List->setVisible(false);
            } else {
                dia_ui.param2Url->setVisible(false);
                QStringList listValues = jobData.value(QLatin1String("param2list")).split(QLatin1String("  "));
                dia_ui.param2List->addItems(listValues);
            }
        } else {
            // Hide param1
            dia_ui.param2Label->setVisible(false);
            dia_ui.param2Url->setVisible(false);
            dia_ui.param2List->setVisible(false);
        }
        if (dia->exec() != QDialog::Accepted) {
            // Abort
            return;
        }
        if (requestParam1) {
            if (param1 == QLatin1String("file")) {
                param1Value = dia_ui.param1Url->url().toLocalFile();
            }
            if (param1 == QLatin1String("frame")) {
                param1Value = QStringLiteral("%tmpfile");
            } else {
                param1Value = dia_ui.param1List->currentText();
            }
            jobData.insert(QLatin1String("param1value"), param1Value);
        }
        if (requestParam2) {
            if (param2 == QLatin1String("file")) {
                param2Value = dia_ui.param2Url->url().toLocalFile();
            } else {
                param2Value = dia_ui.param2List->currentText();
            }
            jobData.insert(QLatin1String("param2value"), param2Value);
        }
    }
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
            owner = ObjectId(KdenliveObjectType::BinClip, binData.first().toInt(), QUuid());
            in = binData.at(1).toInt();
            out = binData.at(2).toInt();
        } else {
            // Process full clip
            owner = ObjectId(KdenliveObjectType::BinClip, id.toInt(), QUuid());
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
    m_clipPointer = binClip.get();
    QString source = binClip->url();
    QString folderId = binClip->parent()->clipId();
    const QString binary = m_parameters.value(QLatin1String("binary"));
    if (!QFile::exists(binary)) {
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                  Q_ARG(QString, i18n("Application %1 not found, please update the job settings", binary)),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        return;
    }
    m_isFfmpegJob = QFileInfo(binary).fileName().contains(QLatin1String("ffmpeg"));
    QString jobParameters = m_parameters.value(QLatin1String("parameters"));
    QStringList parameters;
    m_jobDuration = int(binClip->duration().seconds());
    QFileInfo sourceInfo(source);

    // Tell ffmpeg to overwrite, we do the file exist check ourselves
    if (m_isFfmpegJob) {
        parameters << QStringLiteral("-y");
        if (m_inPoint > -1) {
            parameters << QStringLiteral("-ss") << QString::number(GenTime(m_inPoint, pCore->getCurrentFps()).seconds());
        }
        parameters << QStringLiteral("-stats");
    }

    // Get output file name
    QString extension = m_parameters.value(QLatin1String("output"));
    if (extension.isEmpty()) {
        extension = sourceInfo.suffix();
    }
    if (!extension.startsWith(QLatin1Char('.'))) {
        extension.prepend(QLatin1Char('.'));
    }
    const QString destName = sourceInfo.baseName();
    QDir baseDir = sourceInfo.absoluteDir();
    QString destPath = baseDir.absoluteFilePath(destName + extension);
    if (QFileInfo::exists(destPath) || requestedOutput.contains(destPath)) {
        QString fixedName = destName;
        static const QRegularExpression regex(QRegularExpression::anchoredPattern(QStringLiteral(R"(.*-(\d{4})$)")));
        QRegularExpressionMatch match = regex.match(fixedName);
        if (!match.hasMatch()) {
            // if the file has no index suffix, append -0001
            fixedName.append(QString::asprintf("-%04d", 1));
        } else {
            const int currentSuffix = match.captured(1).toInt();
            fixedName.replace(match.capturedStart(1), match.capturedLength(1), QString::asprintf("-%04d", currentSuffix + 1));
        }
        destPath = baseDir.absoluteFilePath(fixedName + extension);
        while (QFileInfo::exists(destPath) || requestedOutput.contains(destPath)) {
            // if the file name has already an index suffix,
            // increase the number
            match = regex.match(fixedName);
            const int currentSuffix = match.captured(1).toInt();
            fixedName.replace(match.capturedStart(1), match.capturedLength(1), QString::asprintf("-%04d", currentSuffix + 1));
            destPath = baseDir.absoluteFilePath(fixedName + extension);
        }
    }
    // Extract frame is necessary
    requestedOutput << destPath;
    QStringList splitParameters;
    QStringList quotedStrings;
    if (jobParameters.contains(QLatin1Char('"'))) {
        quotedStrings = jobParameters.split(QLatin1Char('"'));
    } else if (jobParameters.contains(QLatin1Char('\''))) {
        quotedStrings = jobParameters.split(QLatin1Char('\''));
    }
    if (!quotedStrings.isEmpty()) {
        for (int ix = 0; ix < quotedStrings.size(); ix++) {
            if (ix % 2 == 0) {
                splitParameters << quotedStrings.at(ix).split(QLatin1Char(' '), Qt::SkipEmptyParts);
                continue;
            }
            splitParameters << quotedStrings.at(ix).simplified();
        }
    } else {
        splitParameters = jobParameters.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    }
    parameters << splitParameters;

    bool outputPlaced = false;
    for (auto &p : parameters) {
        // Replace
        if (p.contains(QStringLiteral("{output}"))) {
            p.replace(QStringLiteral("{output}"), destPath);
            outputPlaced = true;
        }
        if (p.contains(QStringLiteral("{source}"))) {
            p.replace(QStringLiteral("{source}"), source);
        }
        if (p.contains(QStringLiteral("{param1}"))) {
            QString param1Value = m_parameters.value(QLatin1String("param1value"));
            if (param1Value == QStringLiteral("%tmpfile")) {
                if (m_tmpFrameFile.open()) {
                    param1Value = m_tmpFrameFile.fileName();
                    m_tmpFrameFile.close();
                    // Extract frame to file
                    qDebug() << "=====================\nEXTRACTING FRAME TO: " << param1Value << "\n\n==============================";
                    pCore->getMonitor(Kdenlive::ClipMonitor)->extractFrame(param1Value);
                }
            }
            p.replace(QStringLiteral("{param1}"), param1Value);
        }
        if (p.contains(QStringLiteral("{param2}"))) {
            p.replace(QStringLiteral("{param2}"), m_parameters.value(QLatin1String("param2value")));
        }
        if (p.startsWith(QLatin1Char('"')) && p.endsWith(QLatin1Char('"'))) {
            p.remove(0, 1);
            p.chop(1);
        } else if (p.startsWith(QLatin1Char('\'')) && p.endsWith(QLatin1Char('\''))) {
            p.remove(0, 1);
            p.chop(1);
        }
    }
    if (!outputPlaced) {
        parameters << destPath;
    }

    if (m_isFfmpegJob && m_outPoint > -1) {
        int inputIndex = parameters.indexOf(source);
        if (inputIndex > -1) {
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

    qDebug() << "/// CUSTOM TASK PARAMS:\n" << parameters << "\n------";
    m_jobProcess.reset(new QProcess);
    // m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(this, &CustomJobTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &CustomJobTask::processLogInfo);
    m_jobProcess->start(binary, parameters, QIODevice::ReadOnly);
    AbstractTask::setPreferredPriority(m_jobProcess->processId());
    m_jobProcess->waitForFinished(-1);
    bool result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    requestedOutput.removeAll(destPath);
    // remove temporary playlist if it exists
    m_progress = 100;
    QMetaObject::invokeMethod(m_clipPointer, "updateJobProgress");
    if (result) {
        if (QFileInfo(destPath).size() == 0) {
            QFile::remove(destPath);
            // File was not created
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to create file.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
        } else {
            QStringList lutExtentions = {QLatin1String("cube"), QLatin1String("3dl"), QLatin1String("dat"),
                                         QLatin1String("m3d"),  QLatin1String("csp"), QLatin1String("interp")};
            const QString ext = destPath.section(QLatin1Char('.'), -1);
            if (lutExtentions.contains(ext)) {
                QMap<QString, QString> params;
                params.insert(QStringLiteral("av.file"), destPath);
                QMetaObject::invokeMethod(pCore->bin(), "addFilterToClip", Qt::QueuedConnection, Q_ARG(QString, QString::number(m_owner.itemId)),
                                          Q_ARG(QString, QStringLiteral("avfilter.lut3d")), Q_ARG(stringMap, params));
            } else {
                QMetaObject::invokeMethod(pCore->bin(), "addProjectClipInFolder", Qt::QueuedConnection, Q_ARG(QString, destPath),
                                          Q_ARG(QString, QString::number(m_owner.itemId)), Q_ARG(QString, folderId), Q_ARG(QString, m_jobId));
            }
        }
    } else {
        // Process crashed
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
            QMetaObject::invokeMethod(m_clipPointer, "updateJobProgress");
            // emit jobProgress(int(100.0 * progress / m_jobDuration));
        }
    } else {
        // Parse MLT output
        if (buffer.contains(QLatin1String("percentage:"))) {
            m_progress = buffer.section(QStringLiteral("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
            QMetaObject::invokeMethod(m_clipPointer, "updateJobProgress");
        }
    }
}
