/*
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2017  Nicolas Carion
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

#include "jobmanager.h"
#include "bin/abstractprojectitem.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "macros.hpp"
#include "undohelper.hpp"

#include <KMessageWidget>
#include <QFuture>
#include <QFutureWatcher>
#include <QThread>

int JobManager::m_currentId = 0;
JobManager::JobManager(QObject *parent)
    : QAbstractListModel(parent)
    , m_lock(QReadWriteLock::Recursive)
{
}

JobManager::~JobManager()
{
    //slotCancelJobs();
}

int JobManager::getBlockingJobId(const QString &id, AbstractClipJob::JOBTYPE type)
{
    READ_LOCK();
    std::vector<int> result;
    if (m_jobsByClip.count(id) > 0) {
        for (int jobId : m_jobsByClip.at(id)) {
            if (!m_jobs.at(jobId)->m_future.isFinished() && !m_jobs.at(jobId)->m_future.isCanceled()) {
                if (type == AbstractClipJob::NOJOBTYPE || m_jobs.at(jobId)->m_type == type) {
                    return jobId;
                }
            }
        }
    }
    return -1;
}

std::vector<int> JobManager::getPendingJobsIds(const QString &id, AbstractClipJob::JOBTYPE type)
{
    READ_LOCK();
    std::vector<int> result;
    if (m_jobsByClip.count(id) > 0) {
        for (int jobId : m_jobsByClip.at(id)) {
            if (!m_jobs.at(jobId)->m_future.isFinished() && !m_jobs.at(jobId)->m_future.isCanceled()) {
                if (type == AbstractClipJob::NOJOBTYPE || m_jobs.at(jobId)->m_type == type) {
                    result.push_back(jobId);
                }
            }
        }
    }
    return result;
}

std::vector<int> JobManager::getFinishedJobsIds(const QString &id, AbstractClipJob::JOBTYPE type)
{
    READ_LOCK();
    std::vector<int> result;
    if (m_jobsByClip.count(id) > 0) {
        for (int jobId : m_jobsByClip.at(id)) {
            if (m_jobs.at(jobId)->m_future.isFinished() || m_jobs.at(jobId)->m_future.isCanceled()) {
                if (type == AbstractClipJob::NOJOBTYPE || m_jobs.at(jobId)->m_type == type) {
                    result.push_back(jobId);
                }
            }
        }
    }
    return result;
}

void JobManager::discardJobs(const QString &binId, AbstractClipJob::JOBTYPE type)
{
    QWriteLocker locker(&m_lock);
    if (m_jobsByClip.count(binId) == 0) {
        return;
    }
    for (int jobId : m_jobsByClip.at(binId)) {
        if (type == AbstractClipJob::NOJOBTYPE || m_jobs.at(jobId)->m_type == type) {
            for (const std::shared_ptr<AbstractClipJob> &job : m_jobs.at(jobId)->m_job) {
                emit job->jobCanceled();
            }
            m_jobs.at(jobId)->m_future.cancel();
        }
    }
}

bool JobManager::hasPendingJob(const QString &clipId, AbstractClipJob::JOBTYPE type, int *foundId)
{
    READ_LOCK();
    if (m_jobsByClip.count(clipId) > 0) {
        for (int jobId : m_jobsByClip.at(clipId)) {
            if ((type == AbstractClipJob::NOJOBTYPE || m_jobs.at(jobId)->m_type == type) && !m_jobs.at(jobId)->m_future.isFinished() &&
                !m_jobs.at(jobId)->m_future.isCanceled()) {
                if (foundId) {
                    *foundId = jobId;
                }
                return true;
            }
        }
        if (foundId) {
            *foundId = -1;
        }
    }
    return false;
}

void JobManager::updateJobCount()
{
    READ_LOCK();
    int count = 0;
    for (const auto &j : m_jobs) {
        if (!j.second->m_future.isFinished() && !j.second->m_future.isCanceled()) {
            count++;
            /*for (int i = 0; i < j.second->m_future.future().resultCount(); ++i) {
                if (j.second->m_future.future().isResultReadyAt(i)) {
                    count++;
                }
            }*/
        }
    }
    // Set jobs count
    emit jobCount(count);
}

/*
void JobManager::prepareJobs(const QList<ProjectClip *> &clips, double fps, AbstractClipJob::JOBTYPE jobType, const QStringList &params)
{
    // TODO filter clips
    QList<ProjectClip *> matching = filterClips(clips, jobType, params);
    if (matching.isEmpty()) {
        m_bin->doDisplayMessage(i18n("No valid clip to process"), KMessageWidget::Information);
        return;
    }
    QHash<ProjectClip *, AbstractClipJob *> jobs;
    if (jobType == AbstractClipJob::TRANSCODEJOB) {
        jobs = CutClipJob::prepareTranscodeJob(fps, matching, params);
    } else if (jobType == AbstractClipJob::CUTJOB) {
        ProjectClip *clip = matching.constFirst();
        double originalFps = clip->getOriginalFps();
        jobs = CutClipJob::prepareCutClipJob(fps, originalFps, clip);
    } else if (jobType == AbstractClipJob::ANALYSECLIPJOB) {
        jobs = CutClipJob::prepareAnalyseJob(fps, matching, params);
    } else if (jobType == AbstractClipJob::FILTERCLIPJOB) {
        jobs = FilterJob::prepareJob(matching, params);
    } else if (jobType == AbstractClipJob::PROXYJOB) {
        jobs = ProxyJob::prepareJob(m_bin, matching);
    }
    if (!jobs.isEmpty()) {
        QHashIterator<ProjectClip *, AbstractClipJob *> i(jobs);
        while (i.hasNext()) {
            i.next();
            launchJob(i.key(), i.value(), false);
        }
        slotCheckJobProcess();
    }
}
*/

void JobManager::slotDiscardClipJobs(const QString &binId)
{
    QWriteLocker locker(&m_lock);
    if (m_jobsByClip.count(binId) > 0) {
        for (int jobId : m_jobsByClip.at(binId)) {
            Q_ASSERT(m_jobs.count(jobId) > 0);
            for (const std::shared_ptr<AbstractClipJob> &job : m_jobs.at(jobId)->m_job) {
                emit job->jobCanceled();
            }
            m_jobs[jobId]->m_future.cancel();
        }
    }
}

void JobManager::slotCancelPendingJobs()
{
    QWriteLocker locker(&m_lock);
    for (const auto &j : m_jobs) {
        if (!j.second->m_future.isStarted()) {
            for (const std::shared_ptr<AbstractClipJob> &job : j.second->m_job) {
                emit job->jobCanceled();
            }
            j.second->m_future.cancel();
        }
    }
}

void JobManager::slotCancelJobs()
{
    QWriteLocker locker(&m_lock);
    for (const auto &j : m_jobs) {
        if (j.second->m_processed) {
            continue;
        }
        for (const std::shared_ptr<AbstractClipJob> &job : j.second->m_job) {
            emit job->jobCanceled();
        }
        j.second->m_future.cancel();
    }
}

void JobManager::createJob(const std::shared_ptr<Job_t> &job)
{
    // connect progress signals
    QReadLocker locker(&m_lock);
    for (const auto &it : job->m_indices) {
        size_t i = it.second;
        auto binId = it.first;
        connect(job->m_job[i].get(), &AbstractClipJob::jobProgress, [job, i, binId](int p) {
            job->m_progress[i] = std::max(job->m_progress[i], p);
            if (pCore) {
                pCore->projectItemModel()->onItemUpdated(binId, AbstractProjectItem::JobProgress);
            }
        });
    }
    connect(&job->m_future, &QFutureWatcher<bool>::started, this, &JobManager::updateJobCount);
    connect(&job->m_future, &QFutureWatcher<bool>::finished, this, [this, id = job->m_id]() { if (m_jobs.count(id)> 0) slotManageFinishedJob(id); });
    connect(&job->m_future, &QFutureWatcher<bool>::canceled, this, [this, id = job->m_id]() { slotManageCanceledJob(id); });
    job->m_actualFuture = QtConcurrent::mapped(job->m_job, AbstractClipJob::execute);
    job->m_future.setFuture(job->m_actualFuture);
}

void JobManager::slotManageCanceledJob(int id)
{
    qDebug() << "################### JOB canceled: " << id;
    QReadLocker locker(&m_lock);
    if (m_jobs.count(id) == 0) {
        // Job finished, nothing to do
        return;
    }
    if (m_jobs[id]->m_processed) return;
    m_jobs[id]->m_processed = true;
    //m_jobs[id]->m_completionMutex.unlock(); // crashing on Windows
    // send notification to refresh view
    for (const auto &it : m_jobs[id]->m_indices) {
        pCore->projectItemModel()->onItemUpdated(it.first, AbstractProjectItem::JobStatus);
        m_jobsByClip.erase(it.first);
    }
    if (m_jobsByParents.count(id) > 0) {
        m_jobsByParents.erase(id);
    }
    m_jobs.erase(id);
    updateJobCount();
}
void JobManager::slotManageFinishedJob(int id)
{
    qDebug() << "################### JOB finished: " << id;
    QReadLocker locker(&m_lock);
    Q_ASSERT(m_jobs.count(id) > 0);
    if (m_jobs[id]->m_processed) return;

    // send notification to refresh view
    for (const auto &it : m_jobs[id]->m_indices) {
        pCore->projectItemModel()->onItemUpdated(it.first, AbstractProjectItem::JobStatus);
        if (m_jobsByClip.count(it.first) > 0) {
            m_jobsByClip.at(it.first).erase(std::remove(m_jobsByClip.at(it.first).begin(), m_jobsByClip.at(it.first).end(), id), m_jobsByClip.at(it.first).end());
        }
    }
    bool ok = true;
    for (bool res : m_jobs[id]->m_future.future()) {
        ok = ok && res;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (!ok) {
        qDebug() << " * * * ** * * *\nWARNING + + +\nJOB NOT CORRECT FINISH: " << id <<"\n------------------------";
        // TODO: delete child jobs
        m_jobs[id]->m_completionMutex.unlock();
        locker.unlock();
        if (m_jobs.at(id)->m_type == AbstractClipJob::LOADJOB) {
            // loading failed, remove clip
            for (const auto &it : m_jobs[id]->m_indices) {
                std::shared_ptr<AbstractProjectItem> item = pCore->projectItemModel()->getItemByBinId(it.first);
                if (item && item->itemType() == AbstractProjectItem::ClipItem) {
                    auto clipItem = std::static_pointer_cast<ProjectClip>(item);
                    if (!clipItem->isReady()) {
                        // We were trying to load a new clip, delete it
                        pCore->projectItemModel()->requestBinClipDeletion(item, undo, redo);
                    }
                }
            }
        } else {
            const QString bid = m_jobs.at(id)->m_indices.cbegin()->first;
            QPair<QString, QString> message = getJobMessageForClip(id, bid);
            if (!message.first.isEmpty()) {
                if (!message.second.isEmpty()) {
                    pCore->displayBinLogMessage(message.first, KMessageWidget::Warning, message.second);
                } else {
                    pCore->displayBinMessage(message.first, KMessageWidget::Warning);
                }
            }
        }
        m_jobs.erase(id);
        updateJobCount();
        return;
    }
    // unlock mutex to allow further processing
    // TODO: the lock mechanism should handle this better!
    locker.unlock();
    for (const auto &j : m_jobs[id]->m_job) {
        ok = ok && j->commitResult(undo, redo);
    }
    m_jobs[id]->m_processed = true;
    if (!ok) {
        m_jobs[id]->m_failed = true;
        const QString bid = m_jobs.at(id)->m_indices.cbegin()->first;
        QPair<QString, QString> message = getJobMessageForClip(id, bid);
        qDebug()<<"MESSAGE LOG: "<<message;
        if (!message.first.isEmpty()) {
            if (!message.second.isEmpty()) {
                pCore->displayBinLogMessage(message.first, KMessageWidget::Warning, message.second);
            } else {
                pCore->displayBinMessage(message.first, KMessageWidget::Warning);
            }
        }
    }
    m_jobs[id]->m_completionMutex.unlock();
    if (ok && !m_jobs[id]->m_undoString.isEmpty()) {
        pCore->pushUndo(undo, redo, m_jobs[id]->m_undoString);
    }
    if (m_jobsByParents.count(id) > 0) {
        std::vector<int> children = m_jobsByParents[id];
        for (int cid : children) {
            if (!m_jobs[cid]->m_processed) {
                QtConcurrent::run(this, &JobManager::createJob, m_jobs[cid]);
            }
        }
        m_jobsByParents.erase(id);
    }
    m_jobs.erase(id);
    updateJobCount();
}

AbstractClipJob::JOBTYPE JobManager::getJobType(int jobId) const
{
    READ_LOCK();
    Q_ASSERT(m_jobs.count(jobId) > 0);
    return m_jobs.at(jobId)->m_type;
}

JobManagerStatus JobManager::getJobStatus(int jobId) const
{
    READ_LOCK();
    Q_ASSERT(m_jobs.count(jobId) > 0);
    auto job = m_jobs.at(jobId);
    if (job->m_future.isFinished()) {
        return JobManagerStatus::Finished;
    }
    if (job->m_future.isCanceled()) {
        return JobManagerStatus::Canceled;
    }
    if (job->m_future.isRunning()) {
        return JobManagerStatus::Running;
    }
    return JobManagerStatus::Pending;
}

bool JobManager::jobSucceded(int jobId) const
{
    READ_LOCK();
    Q_ASSERT(m_jobs.count(jobId) > 0);
    auto job = m_jobs.at(jobId);
    return !job->m_failed;
}

int JobManager::getJobProgressForClip(int jobId, const QString &binId) const
{
    READ_LOCK();
    Q_ASSERT(m_jobs.count(jobId) > 0);
    auto job = m_jobs.at(jobId);
    Q_ASSERT(job->m_indices.count(binId) > 0);
    size_t ind = job->m_indices.at(binId);
    return job->m_progress[ind];
}

QPair<QString, QString> JobManager::getJobMessageForClip(int jobId, const QString &binId) const
{
    READ_LOCK();
    Q_ASSERT(m_jobs.count(jobId) > 0);
    auto job = m_jobs.at(jobId);
    Q_ASSERT(job->m_indices.count(binId) > 0);
    size_t ind = job->m_indices.at(binId);
    return {job->m_job[ind]->getErrorMessage(), job->m_job[ind]->getLogDetails()};
}

QVariant JobManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    int row = index.row();
    if (row >= int(m_jobs.size()) || row < 0) {
        return QVariant();
    }
    auto it = m_jobs.begin();
    std::advance(it, row);
    switch (role) {
    case Qt::DisplayRole:
        return QVariant(it->second->m_job.front()->getDescription());
        break;
    }
    return QVariant();
}

int JobManager::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return int(m_jobs.size());
}
