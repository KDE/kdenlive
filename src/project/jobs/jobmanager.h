/*
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#include "definitions.h"
#include "abstractclipjob.h"

#include <QObject>
#include <QMutex>
#include <QFutureSynchronizer>

class AbstractClipJob;
class Bin;
class ProjectClip;

/**
 * @class JobManager
 * @brief This class is responsible for clip jobs management.
 *
 */

class JobManager : public QObject
{
    Q_OBJECT

public:
    explicit JobManager(Bin *bin);
    virtual ~JobManager();

    /** @brief Discard specific job type for a clip.
     *  @param id the clip id
     *  @param type The type of job that you want to abort, leave to NOJOBTYPE to abort all jobs
     */
    void discardJobs(const QString &id, AbstractClipJob::JOBTYPE type = AbstractClipJob::NOJOBTYPE);

    /** @brief Check if there is a pending / running job a clip.
     *  @param id the clip id
     *  @param type The type of job that you want to query
     */
    bool hasPendingJob(const QString &clipId, AbstractClipJob::JOBTYPE type);

    /** @brief Launch an MLT filter job that was requested by an effect applied on a timeline clip
     *  @param clip the bin clip
     *  @param producerParams the parameters for the MLT Producer
     *  @param filterParams the parameters for the MLT Filter
     *  @param consumerParams the parameters for the MLT consumer
     *  @param extraParams parameters that will tell the job what to do when finished
     */
    void prepareJobFromTimeline(ProjectClip *clip, const QMap<QString, QString> &producerParams, const QMap<QString, QString> &filterParams, const QMap<QString, QString> &consumerParams, const QMap<QString, QString> &extraParams);

    /** @brief Get ready to process selected job
     *  @param clips the list of selected clips
     *  @param jobType the jobtype requested
     *  @param type the parameters for the job
     */
    void prepareJobs(const QList<ProjectClip *> &clips, double fps, AbstractClipJob::JOBTYPE jobType, const QStringList &params = QStringList());

    /** @brief Filter a list of selected clips to keep only those that match the job type
     *  @param clips the list of selected clips
     *  @param jobType the jobtype requested
     *  @param type the parameters for the job, might be relevant to filter clips
     *  @returns A QMap list (id, urls) of clip that match the job type
     */
    QList<ProjectClip *> filterClips(const QList<ProjectClip *> &clips, AbstractClipJob::JOBTYPE jobType, const QStringList &params);

    /** @brief Put the job in our queue.
     *  @param clip the clip to whom the job will be applied
     *  @param job the job
     *  @param runQueue If true, try to start the job right now. False waits for a later command to start processing, useful when adding many jobs quickly.
     */
    void launchJob(ProjectClip *clip, AbstractClipJob *job, bool runQueue = true);

    /** @brief Get the list of job names for current clip. */
    QStringList getPendingJobs(const QString &id);

private slots:
    void slotCheckJobProcess();
    void slotProcessJobs();
    void slotProcessLog(const QString &id, int progress, int type, const QString &message);

public slots:
    /** @brief Discard jobs running on a clip whose id is in the calling action's data. */
    void slotDiscardClipJobs();
    /** @brief Discard all running jobs. */
    void slotCancelJobs();
    /** @brief Discard all pending jobs. */
    void slotCancelPendingJobs();

private:
    /** @brief A pointer to the project's bin. */
    Bin *m_bin;
    /** @brief Mutex preventing thread issues. */
    QMutex m_jobMutex;
    /** @brief Holds a list of active jobs. */
    QList<AbstractClipJob *> m_jobList;
    /** @brief Holds the threads running a job. */
    QFutureSynchronizer<void> m_jobThreads;
    /** @brief Set to true to trigger abortion of all jobs. */
    bool m_abortAllJobs;
    /** @brief Create a proxy for a clip. */
    void createProxy(const QString &id);
    /** @brief Update job count in info widget. */
    void updateJobCount();

signals:
    void addClip(const QString &, int folderId);
    void processLog(const QString &, int, int, const QString & = QString());
    void updateJobStatus(const QString &, int, int, const QString &label = QString(), const QString &actionName = QString(), const QString &details = QString());
    void gotFilterJobResults(const QString &, int, int, stringMap, stringMap);
    void jobCount(int);
    void checkJobProcess();
};

#endif

