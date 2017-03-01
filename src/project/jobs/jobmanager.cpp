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

#include "jobmanager.h"
#include "kdenlivesettings.h"
#include "doc/kdenlivedoc.h"
#include "abstractclipjob.h"
#include "proxyclipjob.h"
#include "cutclipjob.h"
#include "bin/projectclip.h"
#include "project/clipstabilize.h"
#include "meltjob.h"
#include "filterjob.h"
#include "bin/bin.h"
#include "mlt++/Mlt.h"

#include "kdenlive_debug.h"
#include <QtConcurrent>

#include <KMessageWidget>
#include <klocalizedstring.h>
#include "ui_scenecutdialog_ui.h"

JobManager::JobManager(Bin *bin): QObject()
    , m_bin(bin)
    , m_abortAllJobs(false)
{
    connect(this, &JobManager::processLog, this, &JobManager::slotProcessLog);
    connect(this, &JobManager::checkJobProcess, this, &JobManager::slotCheckJobProcess);
}

JobManager::~JobManager()
{
    m_abortAllJobs = true;
    for (int i = 0; i < m_jobList.count(); ++i) {
        m_jobList.at(i)->setStatus(JobAborted);
    }
    m_jobThreads.waitForFinished();
    m_jobThreads.clearFutures();
    if (!m_jobList.isEmpty()) {
        qDeleteAll(m_jobList);
    }
    m_jobList.clear();
}

void JobManager::slotProcessLog(const QString &id, int progress, int type, const QString &message)
{
    ProjectClip *item = m_bin->getBinClip(id);
    item->setJobStatus((AbstractClipJob::JOBTYPE) type, JobWorking, progress, message);
}

QStringList JobManager::getPendingJobs(const QString &id)
{
    QStringList result;
    QMutexLocker lock(&m_jobMutex);
    for (int i = 0; i < m_jobList.count(); ++i) {
        if (m_jobList.at(i)->clipId() == id && (m_jobList.at(i)->status() == JobWaiting || m_jobList.at(i)->status() == JobWorking)) {
            // discard this job
            result << m_jobList.at(i)->description;
        }
    }
    return result;
}

void JobManager::discardJobs(const QString &id, AbstractClipJob::JOBTYPE type)
{
    QMutexLocker lock(&m_jobMutex);
    for (int i = 0; i < m_jobList.count(); ++i) {
        if (m_jobList.at(i)->clipId() == id && (type == AbstractClipJob::NOJOBTYPE || m_jobList.at(i)->jobType == type)) {
            // discard this job
            m_jobList.at(i)->setStatus(JobAborted);
        }
    }
    emit updateJobStatus(id, type, JobAborted);
    updateJobCount();
}

bool JobManager::hasPendingJob(const QString &clipId, AbstractClipJob::JOBTYPE type)
{
    QMutexLocker lock(&m_jobMutex);
    for (int i = 0; i < m_jobList.count(); i++) {
        if (m_abortAllJobs) {
            break;
        }
        AbstractClipJob *job = m_jobList.at(i);
        if (job->clipId() == clipId && job->jobType == type && (job->status() == JobWaiting || job->status() == JobWorking)) {
            return true;
        }
    }
    return false;
}

void JobManager::slotCheckJobProcess()
{
    if (!m_jobThreads.futures().isEmpty()) {
        // Remove inactive threads
        QList<QFuture<void> > futures = m_jobThreads.futures();
        m_jobThreads.clearFutures();
        for (int i = 0; i < futures.count(); ++i)
            if (!futures.at(i).isFinished()) {
                m_jobThreads.addFuture(futures.at(i));
            }
    }
    if (m_jobList.isEmpty()) {
        return;
    }

    m_jobMutex.lock();
    int count = 0;
    for (int i = 0; i < m_jobList.count(); ++i) {
        if (m_jobList.at(i)->status() == JobWorking || m_jobList.at(i)->status() == JobWaiting) {
            count ++;
        } else {
            // remove finished jobs
            AbstractClipJob *job = m_jobList.takeAt(i);
            job->deleteLater();
            --i;
        }
    }
    m_jobMutex.unlock();
    emit jobCount(count);
    if (m_jobThreads.futures().isEmpty() || m_jobThreads.futures().count() < KdenliveSettings::proxythreads()) {
        m_jobThreads.addFuture(QtConcurrent::run(this, &JobManager::slotProcessJobs));
    }
}

void JobManager::updateJobCount()
{
    int count = 0;
    for (int i = 0; i < m_jobList.count(); ++i) {
        if (m_jobList.at(i)->status() == JobWaiting || m_jobList.at(i)->status() == JobWorking) {
            count ++;
        }
    }
    // Set jobs count
    emit jobCount(count);
}

void JobManager::slotProcessJobs()
{
    bool firstPass = true;
    while (!m_jobList.isEmpty() && !m_abortAllJobs) {
        AbstractClipJob *job = nullptr;
        m_jobMutex.lock();
        for (int i = 0; i < m_jobList.count(); ++i) {
            if (m_jobList.at(i)->status() == JobWaiting) {
                job = m_jobList.at(i);
                job->setStatus(JobWorking);
                break;
            }
        }
        if (!firstPass) {
            updateJobCount();
        }
        m_jobMutex.unlock();

        if (job == nullptr) {
            break;
        }
        firstPass = false;
        QString destination = job->destination();
        // Check if the clip is still here
        ProjectClip *currentClip = m_bin->getBinClip(job->clipId());
        if (currentClip == nullptr) {
            job->setStatus(JobDone);
            continue;
        }
        // Set clip status to started
        currentClip->setJobStatus(job->jobType, job->status());

        // Make sure destination path is writable
        if (!destination.isEmpty()) {
            QFileInfo file(destination);
            bool writable = false;
            if (file.exists()) {
                if (file.isWritable()) {
                    writable = true;
                }
            } else {
                QDir dir = file.absoluteDir();
                if (!dir.exists()) {
                    writable = dir.mkpath(QStringLiteral("."));
                } else {
                    QFileInfo dinfo(dir.absolutePath());
                    writable = dinfo.isWritable();
                }
            }
            if (!writable) {
                emit updateJobStatus(job->clipId(), job->jobType, JobCrashed, i18n("Cannot write to path: %1", destination));
                job->setStatus(JobCrashed);
                continue;
            }
        }
        connect(job, SIGNAL(jobProgress(QString, int, int)), this, SIGNAL(processLog(QString, int, int)));
        connect(job, &AbstractClipJob::cancelRunningJob, m_bin, &Bin::slotCancelRunningJob);

        if (job->jobType == AbstractClipJob::MLTJOB || job->jobType == AbstractClipJob::ANALYSECLIPJOB) {
            connect(job, SIGNAL(gotFilterJobResults(QString, int, int, stringMap, stringMap)), this, SIGNAL(gotFilterJobResults(QString, int, int, stringMap, stringMap)));
        }
        job->startJob();
        if (job->status() == JobDone) {
            emit updateJobStatus(job->clipId(), job->jobType, JobDone);
            //TODO: replace with more generic clip replacement framework
            if (job->jobType == AbstractClipJob::PROXYJOB) {
                m_bin->gotProxy(job->clipId(), destination);
            } else if (job->addClipToProject() > -100) {
                emit addClip(destination, job->addClipToProject());
            }
        } else if (job->status() == JobCrashed || job->status() == JobAborted) {
            emit updateJobStatus(job->clipId(), job->jobType, job->status(), job->errorMessage(), QString(), job->logDetails());
        }
    }
    // Thread finished, cleanup & update count
    QTimer::singleShot(200, this, &JobManager::checkJobProcess);
}

QList<ProjectClip *> JobManager::filterClips(const QList<ProjectClip *> &clips, AbstractClipJob::JOBTYPE jobType, const QStringList &params)
{
    //TODO: filter depending on clip type
    if (jobType == AbstractClipJob::TRANSCODEJOB || jobType == AbstractClipJob::CUTJOB) {
        return CutClipJob::filterClips(clips, params);
    } else if (jobType == AbstractClipJob::FILTERCLIPJOB) {
        return FilterJob::filterClips(clips, params);
    } else if (jobType == AbstractClipJob::PROXYJOB) {
        return ProxyJob::filterClips(clips);
    }
    return QList<ProjectClip *> ();
}

void JobManager::prepareJobFromTimeline(ProjectClip *clip, const QMap<QString, QString> &producerParams, const QMap<QString, QString> &filterParams, const QMap<QString, QString> &consumerParams, const QMap<QString, QString> &extraParams)
{
    MeltJob *job = new MeltJob(clip->clipType(), clip->clipId(), producerParams, filterParams, consumerParams, extraParams);
    job->description = i18n("Filter %1", extraParams.value(QStringLiteral("finalfilter")));
    launchJob(clip, job);
}

void JobManager::prepareJobs(const QList<ProjectClip *> &clips, double fps, AbstractClipJob::JOBTYPE jobType, const QStringList &params)
{
    //TODO filter clips
    QList<ProjectClip *> matching = filterClips(clips, jobType, params);
    if (matching.isEmpty()) {
        m_bin->doDisplayMessage(i18n("No valid clip to process"), KMessageWidget::Information);
        return;
    }
    QHash<ProjectClip *, AbstractClipJob *> jobs;
    if (jobType == AbstractClipJob::TRANSCODEJOB) {
        jobs = CutClipJob::prepareTranscodeJob(fps, matching, params);
    } else if (jobType == AbstractClipJob::CUTJOB) {
        ProjectClip *clip = matching.first();
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

void JobManager::launchJob(ProjectClip *clip, AbstractClipJob *job, bool runQueue)
{
    if (job->isExclusive() && hasPendingJob(clip->clipId(), job->jobType)) {
        delete job;
        return;
    }

    m_jobList.append(job);
    clip->setJobStatus(job->jobType, JobWaiting, 0, job->statusMessage());
    if (runQueue) {
        slotCheckJobProcess();
    }
}

void JobManager::slotDiscardClipJobs()
{
    QAction *act = qobject_cast<QAction *>(sender());
    if (act == nullptr) {
        // Cannot access triggering action, something is wrong
        qCDebug(KDENLIVE_LOG) << "// Error in job action";
        return;
    }
    QString id = act->data().toString();
    if (id.isEmpty()) {
        return;
    }
    discardJobs(id);
}

void JobManager::slotCancelPendingJobs()
{
    QMutexLocker lock(&m_jobMutex);
    for (int i = 0; i < m_jobList.count(); ++i) {
        if (m_jobList.at(i)->status() == JobWaiting) {
            // discard this job
            m_jobList.at(i)->setStatus(JobAborted);
            emit updateJobStatus(m_jobList.at(i)->clipId(), m_jobList.at(i)->jobType, JobAborted);
        }
    }
    updateJobCount();
}

void JobManager::slotCancelJobs()
{
    m_abortAllJobs = true;
    for (int i = 0; i < m_jobList.count(); ++i) {
        m_jobList.at(i)->setStatus(JobAborted);
    }
    m_jobThreads.waitForFinished();
    m_jobThreads.clearFutures();

    //TODO: undo job cancelation ? not sure it's necessary
    /*QUndoCommand *command = new QUndoCommand();
    command->setText(i18np("Cancel job", "Cancel jobs", m_jobList.count()));
    m_jobMutex.lock();
    for (int i = 0; i < m_jobList.count(); ++i) {
        DocClipBase *currentClip = m_doc->clipManager()->getClipById(m_jobList.at(i)->clipId());
        if (!currentClip) continue;
        QMap<QString, QString> newProps = m_jobList.at(i)->cancelProperties();
        if (newProps.isEmpty()) continue;
        QMap<QString, QString> oldProps = currentClip->currentProperties(newProps);
        //TODO
        //new EditClipCommand(this, m_jobList.at(i)->clipId(), oldProps, newProps, true, command);
    }
    m_jobMutex.unlock();
    if (command->childCount() > 0) {
        m_doc->commandStack()->push(command);
    }
    else delete command;
    */
    if (!m_jobList.isEmpty()) {
        qDeleteAll(m_jobList);
    }
    m_jobList.clear();
    m_abortAllJobs = false;
    emit jobCount(0);
}

