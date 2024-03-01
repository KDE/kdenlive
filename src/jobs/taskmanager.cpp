/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "taskmanager.h"
#include "bin/abstractprojectitem.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "undohelper.hpp"

#include <KMessageWidget>
#include <QFuture>
#include <QThread>

TaskManager::TaskManager(QObject *parent)
    : QObject(parent)
    , displayedClip(-1)
    , m_tasksListLock(QReadWriteLock::Recursive)
    , m_blockUpdates(false)
{
    int maxThreads = qMin(4, QThread::idealThreadCount() - 1);
    m_taskPool.setMaxThreadCount(qMax(maxThreads, 1));
    m_transcodePool.setMaxThreadCount(KdenliveSettings::proxythreads());
}

TaskManager::~TaskManager()
{
    slotCancelJobs();
}

bool TaskManager::isBlocked() const
{
    return m_blockUpdates;
}

void TaskManager::updateConcurrency()
{
    m_transcodePool.setMaxThreadCount(KdenliveSettings::proxythreads());
}

void TaskManager::discardJobs(const ObjectId &owner, AbstractTask::JOBTYPE type, bool softDelete, const QVector<AbstractTask::JOBTYPE> exceptions)
{
    qDebug() << "========== READY FOR TASK DISCARD ON: " << owner.itemId;
    if (m_blockUpdates) {
        // We are already deleting all tasks
        return;
    }
    m_tasksListLock.lockForRead();
    // See if there is already a task for this MLT service and resource.
    if (m_taskList.find(owner.itemId) == m_taskList.end()) {
        m_tasksListLock.unlock();
        return;
    }
    std::vector<AbstractTask *> taskList = m_taskList.at(owner.itemId);
    m_tasksListLock.unlock();
    for (AbstractTask *t : taskList) {
        if ((type == AbstractTask::NOJOBTYPE || type == t->m_type) && t->m_progress < 100) {
            // If so, then just add ourselves to be notified upon completion.
            if (exceptions.contains(t->m_type) || t->isCanceled()) {
                // Don't abort
                continue;
            }
            t->cancelJob(softDelete);
            // Block until the task is finished
            t->m_runMutex.lock();
            t->m_runMutex.unlock();
            t->deleteLater();
        }
    }
}

void TaskManager::discardJob(const ObjectId &owner, const QUuid &uuid)
{
    if (m_blockUpdates) {
        // We are already deleting all tasks
        return;
    }
    m_tasksListLock.lockForRead();
    // See if there is already a task for this MLT service and resource.
    if (m_taskList.find(owner.itemId) == m_taskList.end()) {
        m_tasksListLock.unlock();
        return;
    }
    std::vector<AbstractTask *> taskList = m_taskList.at(owner.itemId);
    m_tasksListLock.unlock();
    for (AbstractTask *t : taskList) {
        if ((t->m_uuid == uuid) && t->m_progress < 100 && !t->isCanceled()) {
            t->cancelJob();
            // Block until the task is finished
            t->m_runMutex.lock();
            t->m_runMutex.unlock();
            t->deleteLater();
        }
    }
}

bool TaskManager::hasPendingJob(const ObjectId &owner, AbstractTask::JOBTYPE type) const
{
    QReadLocker lk(&m_tasksListLock);
    if (type == AbstractTask::NOJOBTYPE) {
        // Check for any kind of job for this clip
        return m_taskList.find(owner.itemId) != m_taskList.end();
    }
    if (m_taskList.find(owner.itemId) == m_taskList.end()) {
        return false;
    }
    std::vector<AbstractTask *> taskList = m_taskList.at(owner.itemId);
    for (AbstractTask *t : taskList) {
        if (type == t->m_type && t->m_progress < 100 && !t->m_isCanceled) {
            return true;
        }
    }
    return false;
}

TaskManagerStatus TaskManager::jobStatus(const ObjectId &owner) const
{
    QReadLocker lk(&m_tasksListLock);
    if (m_taskList.find(owner.itemId) == m_taskList.end()) {
        // No job for this clip
        return TaskManagerStatus::NoJob;
    }
    std::vector<AbstractTask *> taskList = m_taskList.at(owner.itemId);
    for (AbstractTask *t : taskList) {
        if (t->m_running) {
            return TaskManagerStatus::Running;
        }
    }
    return TaskManagerStatus::Pending;
}

void TaskManager::updateJobCount()
{
    QReadLocker lk(&m_tasksListLock);
    int count = 0;
    for (const auto &task : m_taskList) {
        count += task.second.size();
    }
    // Set jobs count
    Q_EMIT jobCount(count);
}

void TaskManager::taskDone(int cid, AbstractTask *task)
{
    // This will be executed in the QRunnable job thread
    if (m_blockUpdates) {
        // We are closing, tasks will be handled on close
        return;
    }
    m_tasksListLock.lockForWrite();
    Q_ASSERT(m_taskList.find(cid) != m_taskList.end());
    m_taskList[cid].erase(std::remove(m_taskList[cid].begin(), m_taskList[cid].end(), task), m_taskList[cid].end());
    if (m_taskList[cid].size() == 0) {
        m_taskList.erase(cid);
    }
    task->deleteLater();
    m_tasksListLock.unlock();
    QMetaObject::invokeMethod(this, "updateJobCount");
}

void TaskManager::slotCancelJobs(bool leaveBlocked, const QVector<AbstractTask::JOBTYPE> exceptions)
{
    if (m_blockUpdates) {
        // Already canceling
        return;
    }
    m_tasksListLock.lockForWrite();
    m_blockUpdates = true;
    for (const auto &task : m_taskList) {
        for (AbstractTask *t : task.second) {
            if (m_taskList.find(task.first) != m_taskList.end()) {
                if (!exceptions.contains(t->m_type) && !t->isCanceled()) {
                    // If so, then just add ourselves to be notified upon completion.
                    t->cancelJob();
                    t->m_runMutex.lock();
                    t->m_runMutex.unlock();
                    t->deleteLater();
                }
            }
        }
    }
    if (exceptions.isEmpty()) {
        m_taskPool.waitForDone();
        m_transcodePool.waitForDone();
        m_taskList.clear();
        m_taskPool.clear();
    }
    if (!leaveBlocked) {
        m_blockUpdates = false;
    }
    m_tasksListLock.unlock();
    updateJobCount();
}

void TaskManager::unBlock()
{
    m_blockUpdates = false;
}

void TaskManager::startTask(int ownerId, AbstractTask *task)
{
    if (m_blockUpdates) {
        // We are closing, tasks will be handled on close
        delete task;
        return;
    }
    m_tasksListLock.lockForWrite();
    if (m_taskList.find(ownerId) == m_taskList.end()) {
        // First task for this clip
        m_taskList[ownerId] = {task};
    } else {
        m_taskList[ownerId].emplace_back(task);
    }
    if (task->m_type == AbstractTask::TRANSCODEJOB || task->m_type == AbstractTask::PROXYJOB) {
        // We only want a limited concurrent jobs for those as for example GPU usually only accept 2 concurrent encoding jobs
        m_transcodePool.start(task, task->m_priority);
    } else {
        m_taskPool.start(task, task->m_priority);
    }
    m_tasksListLock.unlock();
    updateJobCount();
}

int TaskManager::getJobProgressForClip(const ObjectId &owner)
{
    QReadLocker lk(&m_tasksListLock);
    QStringList jobNames;
    QList<int> jobsProgress;
    QStringList jobsUuids;
    if (m_taskList.find(owner.itemId) == m_taskList.end()) {
        if (owner.itemId == displayedClip) {
            Q_EMIT detailedProgress(owner, jobNames, jobsProgress, jobsUuids);
        }
        return 100;
    }
    std::vector<AbstractTask *> taskList = m_taskList.at(owner.itemId);
    int cnt = taskList.size();
    if (cnt == 0) {
        return 100;
    }
    int total = 0;
    for (AbstractTask *t : taskList) {
        if (t->m_type == AbstractTask::LOADJOB) {
            // Don't show progress for load task
            cnt--;
        } else if (owner.itemId == displayedClip) {
            jobNames << t->m_description;
            jobsProgress << t->m_progress;
            jobsUuids << t->m_uuid.toString();
        }
        total += t->m_progress;
    }
    if (cnt == 0) {
        return 100;
    }
    total /= cnt;
    if (owner.itemId == displayedClip) {
        Q_EMIT detailedProgress(owner, jobNames, jobsProgress, jobsUuids);
    }
    return total;
}
