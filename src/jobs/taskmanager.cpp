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
    if (m_blockUpdates) {
        // We are already deleting all tasks
        return;
    }
    // See if there is already a task for this MLT service and resource.
    QWriteLocker lk(&m_tasksListLock);
    if (m_taskList.find(owner.itemId) == m_taskList.end()) {
        return;
    }
    std::vector<AbstractTask *> taskList = m_taskList.at(owner.itemId);
    int ix = taskList.size() - 1;
    while (ix >= 0) {
        AbstractTask *t = taskList.at(ix);
        AbstractTask::JOBTYPE taskType = t->m_type;
        // If so, then just add ourselves to be notified upon completion.
        if (exceptions.contains(taskType) || t->isCanceled()) {
            // Don't abort
            ix--;
            continue;
        }
        if ((type != AbstractTask::NOJOBTYPE && type != taskType) || t->m_progress == 100) {
            ix--;
            continue;
        }
        if (taskType != AbstractTask::TRANSCODEJOB && taskType != AbstractTask::PROXYJOB) {
            if (m_taskPool.tryTake(t)) {
                // Task was not started yet, we can simply delete
                m_taskList[owner.itemId].erase(std::remove(m_taskList[owner.itemId].begin(), m_taskList[owner.itemId].end(), t),
                                               m_taskList[owner.itemId].end());
                delete t;
                ix--;
                continue;
            }
        } else {
            if (m_transcodePool.tryTake(t)) {
                // Task was not started yet, we can simply delete
                m_taskList[owner.itemId].erase(std::remove(m_taskList[owner.itemId].begin(), m_taskList[owner.itemId].end(), t),
                                               m_taskList[owner.itemId].end());
                delete t;
                ix--;
                continue;
            }
        }
        if (t->cancelJob(softDelete)) {
            // Block until the task is finished
            m_taskList[owner.itemId].erase(std::remove(m_taskList[owner.itemId].begin(), m_taskList[owner.itemId].end(), t), m_taskList[owner.itemId].end());
            t->m_runMutex.lock();
            t->m_runMutex.unlock();
            t->deleteLater();
        }
        ix--;
    }
}

void TaskManager::discardJob(const ObjectId &owner, const QUuid &uuid)
{
    if (m_blockUpdates) {
        // We are already deleting all tasks
        return;
    }
    // See if there is already a task for this MLT service and resource.
    if (m_taskList.find(owner.itemId) == m_taskList.end()) {
        return;
    }
    QWriteLocker lk(&m_tasksListLock);
    std::vector<AbstractTask *> taskList = m_taskList.at(owner.itemId);
    int ix = taskList.size() - 1;
    while (ix >= 0) {
        AbstractTask *t = taskList.at(ix);
        AbstractTask::JOBTYPE taskType = t->m_type;
        if ((t->m_uuid != uuid) || t->m_progress == 100 || t->isCanceled()) {
            ix--;
            continue;
        }
        if (taskType != AbstractTask::TRANSCODEJOB && taskType != AbstractTask::PROXYJOB) {
            if (m_taskPool.tryTake(t)) {
                // Task was not started yet, we can simply delete
                m_taskList[owner.itemId].erase(std::remove(m_taskList[owner.itemId].begin(), m_taskList[owner.itemId].end(), t),
                                               m_taskList[owner.itemId].end());
                delete t;
                ix--;
                continue;
            }
        } else {
            if (m_transcodePool.tryTake(t)) {
                // Task was not started yet, we can simply delete
                m_taskList[owner.itemId].erase(std::remove(m_taskList[owner.itemId].begin(), m_taskList[owner.itemId].end(), t),
                                               m_taskList[owner.itemId].end());
                delete t;
                ix--;
                continue;
            }
        }
        if (t->cancelJob()) {
            m_taskList[owner.itemId].erase(std::remove(m_taskList[owner.itemId].begin(), m_taskList[owner.itemId].end(), t), m_taskList[owner.itemId].end());
            // Block until the task is finished
            t->m_runMutex.lock();
            t->m_runMutex.unlock();
            t->deleteLater();
        }
        ix--;
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

void TaskManager::taskDone(int cid, AbstractTask *task)
{
    // This will be executed in the QRunnable job thread
    if (m_blockUpdates) {
        // We are closing, tasks will be handled on close
        return;
    }
    m_tasksListLock.lockForWrite();
    if (m_taskList.find(cid) != m_taskList.end()) {
        m_taskList[cid].erase(std::remove(m_taskList[cid].begin(), m_taskList[cid].end(), task), m_taskList[cid].end());
        if (m_taskList[cid].size() == 0) {
            m_taskList.erase(cid);
        }
    }
    int count = 0;
    for (const auto &task : m_taskList) {
        count += task.second.size();
    }
    m_tasksListLock.unlock();
    // Set jobs count
    Q_EMIT jobCount(count);
    task->deleteLater();
}

void TaskManager::slotCancelJobs(bool leaveBlocked, const QVector<AbstractTask::JOBTYPE> exceptions)
{
    if (m_blockUpdates) {
        // Already canceling
        return;
    }
    m_tasksListLock.lockForWrite();
    m_blockUpdates = true;
    qDebug() << "ZZZZZZZZZZZZZZZZZZZZZZZ\n\nSTARTING TASKMANAGER CLOSURE, ACTIVE THREADS: " << m_taskPool.activeThreadCount() << "\n\nZZZZZZZZZZZZZZZZZZZZZZZ";

    for (const auto &task : m_taskList) {
        int ix = task.second.size() - 1;
        qDebug() << "::: CLOSING TASKS : " << task.second.size();
        while (ix >= 0) {
            AbstractTask *t = task.second.at(ix);
            AbstractTask::JOBTYPE taskType = t->m_type;
            if (exceptions.contains(taskType)) {
                ix--;
                continue;
            }
            if (taskType != AbstractTask::TRANSCODEJOB && taskType != AbstractTask::PROXYJOB) {
                if (m_taskPool.tryTake(t)) {
                    // Task was not started yet, we can simply delete
                    qDebug() << "** DELETED  1 TASK from task pool: " << taskType;
                    delete t;
                    ix--;
                    continue;
                }
            } else {
                if (m_transcodePool.tryTake(t)) {
                    // Task was not started yet, we can simply delete
                    qDebug() << "** DELETED  1 TASK from transcode pool: " << taskType;
                    delete t;
                    ix--;
                    continue;
                }
            }
            if (m_taskList.find(task.first) != m_taskList.end()) {
                // If so, then just add ourselves to be notified upon completion.
                qDebug() << "** CLOSING 1 TASK : " << taskType;
                t->cancelJob();
                t->m_runMutex.lock();
                t->m_runMutex.unlock();
                t->deleteLater();
                qDebug() << "** CLOSING 1 TASK DONE : " << taskType;
            }
            ix--;
        }
    }
    m_tasksListLock.unlock();
    qDebug() << "====== 1....";
    if (exceptions.isEmpty()) {
        m_taskList.clear();
        m_taskPool.clear();
        qDebug() << "====== 2....";
        if (!m_taskPool.waitForDone(5000)) {
            qDebug() << "====== FAILED TO TERMINATE ALL TASKS. Currently alive: " << m_taskPool.activeThreadCount();
            Q_ASSERT(false);
        }
        if (!m_transcodePool.waitForDone(5000)) {
            qDebug() << "====== FAILED TO TERMINATE ALL TRANSCODE TASKS. Currently alive: " << m_transcodePool.activeThreadCount();
            Q_ASSERT(false);
        }
        qDebug() << "====== 3....";
    }
    if (!leaveBlocked) {
        // Set jobs count
        Q_EMIT jobCount(0);
        m_blockUpdates = false;
    }
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
    int count = 0;
    for (const auto &task : m_taskList) {
        count += task.second.size();
    }
    m_tasksListLock.unlock();
    // Set jobs count
    Q_EMIT jobCount(count);
    if (task->m_type == AbstractTask::TRANSCODEJOB || task->m_type == AbstractTask::PROXYJOB) {
        // We only want a limited concurrent jobs for those as for example GPU usually only accept 2 concurrent encoding jobs
        m_transcodePool.start(task, task->m_priority);
    } else {
        m_taskPool.start(task, task->m_priority);
    }
}

int TaskManager::getJobProgressForClip(const ObjectId &owner)
{
    QStringList jobNames;
    QList<int> jobsProgress;
    QStringList jobsUuids;
    QReadLocker lk(&m_tasksListLock);
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
        if (t->m_type == AbstractTask::LOADJOB || t->m_type == AbstractTask::THUMBJOB || t->m_progress == 100 || t->m_isCanceled) {
            // Don't show progress for load task or canceled tasks
            cnt--;
        } else if (owner.itemId == displayedClip) {
            jobNames << t->m_description;
            jobsProgress << t->m_progress;
            jobsUuids << t->m_uuid.toString();
        }
        total += t->m_progress;
    }
    lk.unlock();
    if (owner.itemId == displayedClip) {
        Q_EMIT detailedProgress(owner, jobNames, jobsProgress, jobsUuids);
    }
    if (cnt == 0) {
        return 100;
    }
    total /= cnt;
    return total;
}
