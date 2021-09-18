/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/


#pragma once

#include "abstracttask.h"
#include "definitions.h"

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QObject>
#include <QReadWriteLock>
#include <QThreadPool>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

class AbstractTask;

enum class TaskManagerStatus { NoJob, Pending, Running, Finished, Canceled };
Q_DECLARE_METATYPE(TaskManagerStatus)

/** @class TaskManager
    @brief This class is responsible for clip jobs management.
 */
class TaskManager : public QObject
{
    Q_OBJECT

public:
    explicit TaskManager(QObject *parent);
    ~TaskManager() override;

    /** @brief Discard specific job type for a clip.
     *  @param owner the owner item for this task
     *  @param type The type of job that you want to abort, leave to NOJOBTYPE to abort all jobs
     */
    void discardJobs(const ObjectId &owner, AbstractTask::JOBTYPE type = AbstractTask::NOJOBTYPE, bool softDelete = false);

    /** @brief Check if there is a pending / running job a clip.
     *  @param owner the owner item for this task
     *  @param type The type of job that you want to query
     */
    bool hasPendingJob(const ObjectId &owner, AbstractTask::JOBTYPE type = AbstractTask::NOJOBTYPE) const;
    
    TaskManagerStatus jobStatus(const ObjectId &owner) const;

    /** @brief return the progress of a given job on a given clip */
    int getJobProgressForClip(const ObjectId &owner) const;
    
    /** @brief Add a task in the list and push it on the thread pool */
    void startTask(int ownerId, AbstractTask *task);

    /** @brief Remove a finished task */
    void taskDone(int cid, AbstractTask *task);
    
    /** @brief Update the number of concurrent jobs allowed */
    void updateConcurrency();

    /** @brief return the message of a given job on a given clip (message, detailed log)*/
    //QPair<QString, QString> getJobMessageForClip(int jobId, const QString &binId) const;

public slots:
    /** @brief Discard all running jobs. */
    void slotCancelJobs();

private slots:
    /** @brief Update number of running jobs. */
    void updateJobCount();

private:
    QThreadPool m_taskPool;
    QThreadPool m_transcodePool;
    std::unordered_map<int, std::vector<AbstractTask*> > m_taskList;
    mutable QReadWriteLock m_tasksListLock;

signals:
    void jobCount(int);
};
