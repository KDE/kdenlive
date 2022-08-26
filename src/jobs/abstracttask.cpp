/*
    SPDX-FileCopyrightText: 2013-2021 Meltytech LLC
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "abstracttask.h"
#include "audio/audioStreamInfo.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "kdenlivesettings.h"

#ifdef Q_OS_UNIX
// on Unix systems we can use setpriority() to make proxy-rendering tasks lower
// priority
#include "sys/resource.h"
#endif

#ifdef Q_OS_WIN
// on Windows we can use SetPriorityClass() instead
#include <windows.h>
#endif

#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QList>
#include <QProcess>
#include <QRgb>
#include <QString>
#include <QThreadPool>
#include <QTime>
#include <QVariantList>

AbstractTask::AbstractTask(const ObjectId &owner, JOBTYPE type, QObject *object)
    : QRunnable()
    , m_owner(owner)
    , m_object(object)
    , m_progress(0)
    , m_isCanceled(false)
    , m_softDelete(false)
    , m_isForce(false)
    , m_running(false)
    , m_type(type)
{
    setAutoDelete(false);
    switch (type) {
    case AbstractTask::LOADJOB:
        m_priority = 10;
        break;
    case AbstractTask::TRANSCODEJOB:
    case AbstractTask::PROXYJOB:
        m_priority = 8;
        break;
    case AbstractTask::FILTERCLIPJOB:
    case AbstractTask::STABILIZEJOB:
    case AbstractTask::ANALYSECLIPJOB:
    case AbstractTask::SPEEDJOB:
        m_priority = 5;
        break;
    default:
        m_priority = 5;
        break;
    }
}

void AbstractTask::cancelJob(bool softDelete)
{
    if (m_isCanceled.testAndSetAcquire(0, 1)) {
        if (softDelete) {
            m_softDelete.testAndSetAcquire(0, 1);
        }
        qDebug() << "====== SETTING TASK CANCELED: " << m_isCanceled << ", TYPE: " << m_type;
        emit jobCanceled();
    }
}

const ObjectId AbstractTask::ownerId() const
{
    return m_owner;
}

AbstractTask::~AbstractTask() {}

bool AbstractTask::operator==(const AbstractTask &b)
{
    return m_owner == b.ownerId();
}

void AbstractTask::run()
{
    qDebug() << "============0\n\nABSTRACT TASKSTARTRING\n\n==================";
}

// Background tasks should not slow down the main UI too much. Unless the user
// has opted out, lower the priority of proxy and transcode tasks.
void AbstractTask::setPreferredPriority(qint64 pid)
{
    if (!KdenliveSettings::nice_tasks()) {
        qDebug() << "Not changing process priority for PID" << pid;
        return;
    }
#ifdef Q_OS_UNIX
    qDebug() << "Lowering task priority for PID" << pid;
    int err = setpriority(PRIO_PROCESS, pid, 10);
    if (err != 0) {
        qWarning() << "Failed to lower task priority for PID" << pid << " - error" << strerror(errno);
    }
#endif
#ifdef Q_OS_WIN
    HANDLE processHandle = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (processHandle) {
        qDebug() << "Lowering priority for PID" << pid;
        SetPriorityClass(processHandle, BELOW_NORMAL_PRIORITY_CLASS);
        CloseHandle(processHandle);
    } else {
        qWarning() << "Could not get HANDLE for PID" << pid;
    }
#endif
}
