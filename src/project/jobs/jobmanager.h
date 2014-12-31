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
    JobManager(Bin *bin, double fps);
    virtual ~JobManager();

    /** @brief Discard specific job type for a clip. 
     *  @param id the clip id
     *  @param type The type of job that you want to abort, leave to NOJOBTYPE to abort all jobs
     */
    void discardJobs(const QString &id, AbstractClipJob::JOBTYPE type = AbstractClipJob::NOJOBTYPE);

    /** @brief Start a job for a clip. 
     *  @param id the clip id
     *  @param type The type of job that you want to start
     */
    void startJob(const QString &id, AbstractClipJob::JOBTYPE type);

    /** @brief Check if there is a pending / running job a clip. 
     *  @param id the clip id
     *  @param type The type of job that you want to query
     */
    bool hasPendingJob(const QString &clipId, AbstractClipJob::JOBTYPE type);

private slots:
    void slotCheckJobProcess();
    void slotProcessJobs();
    void slotProcessLog(const QString &id, int progress, int type, const QString &message);

private:
    /** @brief A pointer to the project's bin. */
    Bin *m_bin;
    /** @brief Mutex preventing thread issues. */
    QMutex m_jobMutex;
    /** @brief Holds a list of active jobs. */
    QList <AbstractClipJob *> m_jobList;
    /** @brief Holds the threads running a job. */
    QFutureSynchronizer<void> m_jobThreads;
    /** @brief Set to true to trigger abortion of all jobs. */
    bool m_abortAllJobs;
    /** @brief Stores the project's fps, useful for some jobs. */
    double m_fps;
    /** @brief Get the list of job names for current clip. */
    QStringList getPendingJobs(const QString &id);
    void slotStartFilterJob(const ItemInfo &info, const QString&id, QMap <QString, QString> &filterParams, QMap <QString, QString> &consumerParams, QMap <QString, QString> &extraParams);
    void startClipFilterJob(QList <ProjectClip *> clipList, const QString &filterName);
    void processClipJob(QStringList ids, const QString&destination, bool autoAdd, QMap <QString, QString> producerParams, QMap <QString, QString> filterParams, QMap <QString, QString> consumerParams, const QString &description, QMap <QString, QString> extraParams);
    /** @brief Create a proxy for a clip. */
    void createProxy(const QString &id);

signals:
    //TODO connect to some widget to display message
    void displayMessage(const QString&, int progress, MessageType type = DefaultMessage);
    void addClip(const QString, const QString &, const QString &);
    void processLog(const QString&, int , int, const QString & = QString());
    void jobCount(int);
    void checkJobProcess();
};

#endif

