/*
Copyright (C) 2021  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <QFutureWatcher>
#include <QThread>

TaskManager::TaskManager(QObject *parent)
    : QObject(parent)
    , m_tasksListLock(QReadWriteLock::Recursive)
{
    int maxThreads = qMin(4, QThread::idealThreadCount() - 1);
    m_taskPool.setMaxThreadCount(qMax(maxThreads, 1));
    // TODO: make configurable for user to adjust to GPU
    m_transcodePool.setMaxThreadCount(KdenliveSettings::proxythreads());
}

TaskManager::~TaskManager()
{
    slotCancelJobs();
}

void TaskManager::discardJobs(const ObjectId &owner, AbstractTask::JOBTYPE type, bool softDelete)
{
    qDebug()<<"========== READY FOR TASK DELETION ON: "<<owner.second;
    m_tasksListLock.lockForRead();
    // See if there is already a task for this MLT service and resource.
    if (m_taskList.find(owner.second) == m_taskList.end()) {
        m_tasksListLock.unlock();
        return;
    }
    std::vector<AbstractTask*> taskList = m_taskList.at(owner.second);
    m_tasksListLock.unlock();
    for (AbstractTask* t : taskList) {
        if ((type == AbstractTask::NOJOBTYPE || type == t->m_type) && t->m_progress < 100) {
            // If so, then just add ourselves to be notified upon completion.
            t->cancelJob(softDelete);
            qDebug()<<"========== DELETING JOB!!!!";
            // Block until the task is finished
            //t->m_runMutex.lock();
        }
    }
}

bool TaskManager::hasPendingJob(const ObjectId &owner, AbstractTask::JOBTYPE type) const
{
    QReadLocker lk(&m_tasksListLock);
    if (type == AbstractTask::NOJOBTYPE) {
        // Check for any kind of job for this clip
        return m_taskList.find(owner.second) != m_taskList.end();
    }
    if (m_taskList.find(owner.second) == m_taskList.end()) {
        return false;
    }
    std::vector<AbstractTask*> taskList = m_taskList.at(owner.second);
    for (AbstractTask* t : taskList) {
        if (type == t->m_type && t->m_progress < 100) {
            return true;
        }
    }
    return false;
}

TaskManagerStatus TaskManager::jobStatus(const ObjectId &owner) const
{
    QReadLocker lk(&m_tasksListLock);
    if (m_taskList.find(owner.second) == m_taskList.end()) {
        // No job for this clip
        return TaskManagerStatus::NoJob;
    }
    std::vector<AbstractTask*> taskList = m_taskList.at(owner.second);
    for (AbstractTask* t : taskList) {
        if (t->m_running) {
            return TaskManagerStatus::Running;;
        }
    }
    return TaskManagerStatus::Pending;
}

void TaskManager::updateJobCount()
{
    QReadLocker lk(&m_tasksListLock);
    int count = 0;
    for (auto task : m_taskList) {
        count += task.second.size();
    }
    // Set jobs count
    emit jobCount(count);
}

void TaskManager::taskDone(int cid, AbstractTask *task)
{
    // This will be executed in the QRunnable job thread
    m_tasksListLock.lockForWrite();
    Q_ASSERT(m_taskList.find(cid) != m_taskList.end());
    m_taskList[cid].erase(std::remove(m_taskList[cid].begin(), m_taskList[cid].end(), task), m_taskList[cid].end());
    if (m_taskList[cid].size() == 0) {
        m_taskList.erase(cid);
    }
    m_tasksListLock.unlock();
    QMetaObject::invokeMethod(this, "updateJobCount");
}


void TaskManager::slotCancelJobs()
{
    m_tasksListLock.lockForRead();
    // See if there is already a task for this MLT service and resource.
    for (auto task : m_taskList) {
        for (AbstractTask* t : task.second) {
            // If so, then just add ourselves to be notified upon completion.
            t->cancelJob();
        }
    }
    m_tasksListLock.unlock();
    m_taskPool.waitForDone();
    m_transcodePool.waitForDone();
    updateJobCount();
}

void TaskManager::startTask(int ownerId, AbstractTask *task)
{
    m_tasksListLock.lockForWrite();
    qDebug()<<"========== STARTING TASK FOR: "<<ownerId;
    if (m_taskList.find(ownerId) == m_taskList.end()) {
        // First task for this clip
        m_taskList[ownerId] = {task};
    } else {
        m_taskList[ownerId].emplace_back(std::move(task));
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

int TaskManager::getJobProgressForClip(const ObjectId &owner) const
{
    QReadLocker lk(&m_tasksListLock);
    if (m_taskList.find(owner.second) == m_taskList.end()) {
        return 100;
    }
    std::vector<AbstractTask*> taskList = m_taskList.at(owner.second);
    int cnt = taskList.size();
    if (cnt == 0) {
        return 100;
    }
    int total = 0;
    for (AbstractTask* t : taskList) {
        total += t->m_progress;
    }
    total /= cnt;
    return total;
}

/*QPair<QString, QString> TaskManager::getJobMessageForClip(int jobId, const QString &binId) const
{
    READ_LOCK();
    Q_ASSERT(m_jobs.count(jobId) > 0);
    auto job = m_jobs.at(jobId);
    Q_ASSERT(job->m_indices.count(binId) > 0);
    size_t ind = job->m_indices.at(binId);
    return {job->m_job[ind]->getErrorMessage(), job->m_job[ind]->getLogDetails()};
}*/
