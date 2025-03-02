/*
    SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "melttask.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "xml/xml.hpp"

#include <KLocalizedString>
#include <KMessageWidget>
#include <QDir>
#include <QProcess>
#include <QThread>

MeltTask::MeltTask(const ObjectId &owner, const QString &binId, const QString &playlistName, const QStringList &jobArgs, const QString &jobDescription,
                   QObject *object)
    : AbstractTask(owner, AbstractTask::MELTJOB, object)
    , m_binId(binId)
    , m_playlistName(playlistName)
    , m_jobArgs(jobArgs)
{
    m_description = jobDescription;
}

void MeltTask::start(const ObjectId &owner, const QString &binId, const QString &playlistName, const QStringList &jobArgs, const QString &jobDescription,
                     QObject *object, const std::function<void()> &readyCallBack)
{
    MeltTask *task = new MeltTask(owner, binId, playlistName, jobArgs, jobDescription, object);
    connect(task, &MeltTask::taskDone, [readyCallBack]() { QMetaObject::invokeMethod(qApp, [readyCallBack] { readyCallBack(); }); });
    pCore->taskManager.startTask(owner.itemId, task);
}

void MeltTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        return;
    }
    QMutexLocker lock(&m_runMutex);
    m_running = true;

    // TODO: check that playlist exists
    m_jobProcess.reset(new QProcess);
    QObject::connect(this, &AbstractTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
    QObject::connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &MeltTask::processLogInfo);
    m_jobArgs.prepend(QStringLiteral("error"));
    m_jobArgs.prepend(QStringLiteral("-loglevel"));
    qDebug() << "::: // STARTING MELT JOB: " << m_jobArgs << "\nFOR FILE: " << m_playlistName;
    m_jobProcess->start(KdenliveSettings::meltpath(), m_jobArgs);
    m_jobProcess->waitForFinished(-1);
    bool result = m_jobProcess->exitStatus() == QProcess::NormalExit;
    m_progress = 100;
    QMetaObject::invokeMethod(m_object, "updateJobProgress", Q_ARG(ObjectId, m_owner), Q_ARG(int, m_progress));
    if (m_isCanceled || !result) {
        if (!m_isCanceled) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to process playlist.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
        }
    }
    if (m_playlistName.startsWith(QDir::tempPath())) {
        QFile::remove(m_playlistName);
    }
    Q_EMIT taskDone();
}

void MeltTask::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardError());
    m_logDetails.append(buffer);
    // Parse MLT output
    if (buffer.contains(QLatin1String("percentage:"))) {
        int progress = buffer.section(QStringLiteral("percentage:"), 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
        if (progress != m_progress) {
            m_progress = progress;
            QMetaObject::invokeMethod(m_object, "updateJobProgress", Q_ARG(ObjectId, m_owner), Q_ARG(int, m_progress));
        }
    }
}
