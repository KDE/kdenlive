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

#ifndef JOBMANAGER
#define JOBMANAGER

#include "abstractclipjob.h"
#include "definitions.h"

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QObject>
#include <QReadWriteLock>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

class AbstractClipJob;

/**
 * @class JobManager
 * @brief This class is responsible for clip jobs management.
 *
 */

enum class JobManagerStatus { NoJob, Pending, Running, Finished, Canceled };
Q_DECLARE_METATYPE(JobManagerStatus)
struct Job_t
{
    std::vector<std::shared_ptr<AbstractClipJob>> m_job; // List of the jobs
    std::vector<int> m_progress;                         // progress of the job, for each clip
    std::unordered_map<QString, size_t> m_indices;       // keys are binIds, value are ids in the vectors m_job and m_progress;
    QFutureWatcher<bool> m_future;                       // future of the job
    QFuture<bool> m_actualFuture;
    QMutex m_completionMutex; // mutex that is locked during execution of the process
    AbstractClipJob::JOBTYPE m_type;
    QString m_undoString;
    int m_id;
    bool m_processed = false; // flag that we set to true when we are done with this job
    bool m_failed = false;    // flag that we set to true when a problem occurred
};


class JobManager : public QAbstractListModel, public enable_shared_from_this_virtual<JobManager>
{
    Q_OBJECT

public:
    explicit JobManager(QObject *parent);
    ~JobManager() override;

    /** @brief Start a job
        This function calls the prepareJob function of the job if it provides one.
        @param T is the type of job (must inherit from AbstractClipJob)
        @param binIds is the list of clips to which we apply the job
        @param parents is the list of the ids of the job that must terminate before this one can start
        @param args are the arguments to construct the job
        @param return the id of the created job
    */
    template <typename T, typename... Args> int startJob(const std::vector<QString> &binIds, int parentId, QString undoString, Args &&... args);
    // Same function, but we specify the function used to create a new job
    template <typename T, typename... Args>
    int startJob(const std::vector<QString> &binIds, int parentId, QString undoString, std::function<std::shared_ptr<T>(const QString &, Args...)> createFn,
                 Args &&... args);

    // Same function, but do not call prepareJob
    template <typename T, typename... Args> int startJob_noprepare(const std::vector<QString> &binIds, int parentId, QString undoString, Args &&... args);

    /** @brief Discard specific job type for a clip.
     *  @param binId the clip id
     *  @param type The type of job that you want to abort, leave to NOJOBTYPE to abort all jobs
     */
    void discardJobs(const QString &binId, AbstractClipJob::JOBTYPE type = AbstractClipJob::NOJOBTYPE);

    /** @brief Check if there is a pending / running job a clip.
     *  @param binId the clip id
     *  @param type The type of job that you want to query
     *  @param foundId : if a valid ptr is passed, we store the id of the first matching job found (-1 if not found)
     */
    bool hasPendingJob(const QString &binId, AbstractClipJob::JOBTYPE type, int *foundId = nullptr);

    /** @brief Get the list of pending or running job ids for given clip.
     *  @param binId the clip id
     *  @param type The type of job that you want to query. Leave to NOJOBTYPE to match all
     */
    std::vector<int> getPendingJobsIds(const QString &binId, AbstractClipJob::JOBTYPE type = AbstractClipJob::NOJOBTYPE);
    int getBlockingJobId(const QString &id, AbstractClipJob::JOBTYPE type);

    /** @brief Get the list of finished or cancelled job ids for given clip.
     *  @param binId the clip id
     *  @param type The type of job that you want to query. Leave to NOJOBTYPE to match all
     */
    std::vector<int> getFinishedJobsIds(const QString &binId, AbstractClipJob::JOBTYPE type = AbstractClipJob::NOJOBTYPE);

    /** @brief return the type of a given job */
    AbstractClipJob::JOBTYPE getJobType(int jobId) const;

    /** @brief return the type of a given job */
    JobManagerStatus getJobStatus(int jobId) const;

    /** @brief returns false if job failed */
    bool jobSucceded(int jobId) const;

    /** @brief return the progress of a given job on a given clip */
    int getJobProgressForClip(int jobId, const QString &binId) const;

    /** @brief return the message of a given job on a given clip (message, detailed log)*/
    QPair<QString, QString> getJobMessageForClip(int jobId, const QString &binId) const;

    // Mandatory overloads
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

protected:
    // Helper function to launch a given job.
    // This has to be launched asynchrnously since it blocks until all parents are finished
    void createJob(const std::shared_ptr<Job_t> &job);

    void updateJobCount();

    void slotManageCanceledJob(int id);
    void slotManageFinishedJob(int id);

public slots:
    /** @brief Discard jobs running on a given clip */
    void slotDiscardClipJobs(const QString &binId);
    /** @brief Discard all running jobs. */
    void slotCancelJobs();
    /** @brief Discard all pending jobs. */
    void slotCancelPendingJobs();

private:
    /** @brief This is a lock that ensures safety in case of concurrent access */
    mutable QReadWriteLock m_lock;
    /** @brief This is the id of the last created job */
    static int m_currentId;
    /** @brief This is the list of all jobs, ordered by id. A job is represented by a pointer to the job class and a future to the result */
    std::map<int, std::shared_ptr<Job_t>> m_jobs;
    /** @brief List of all the jobs by clip. */
    std::unordered_map<QString, std::vector<int>> m_jobsByClip;
    std::unordered_map<int, std::vector<int>> m_jobsByParents;

signals:
    void jobCount(int);
};

#include "jobmanager.ipp"
#endif
