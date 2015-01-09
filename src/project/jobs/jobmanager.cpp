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

#include <QProcess>
#include <QDialog>
#include <QDebug>
#include <QtConcurrent>


#include <klocalizedstring.h>
#include <KMessageBox>
#include "ui_scenecutdialog_ui.h"

JobManager::JobManager(Bin *bin, double fps): QObject()
  , m_bin(bin)
  , m_fps(fps)
{
    connect(this, SIGNAL(processLog(QString,int,int,QString)), this, SLOT(slotProcessLog(QString,int,int,QString)));
    connect(this, SIGNAL(checkJobProcess()), this, SLOT(slotCheckJobProcess()));
}

JobManager::~JobManager()
{
    m_abortAllJobs = true;
    for (int i = 0; i < m_jobList.count(); ++i) {
        m_jobList.at(i)->setStatus(JobAborted);
    }
    m_jobThreads.waitForFinished();
    m_jobThreads.clearFutures();
    if (!m_jobList.isEmpty()) qDeleteAll(m_jobList);
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
}

bool JobManager::hasPendingJob(const QString &clipId, AbstractClipJob::JOBTYPE type)
{
    QMutexLocker lock(&m_jobMutex);
    for (int i = 0; i < m_jobList.count(); ++i) {
        if (m_abortAllJobs) break;
        AbstractClipJob *job = m_jobList.at(i);
        if (job->clipId() == clipId && job->jobType == type && (job->status() == JobWaiting || job->status() == JobWorking)) return true;
    }

    return false;
}

void JobManager::slotCheckJobProcess()
{
    if (!m_jobThreads.futures().isEmpty()) {
        // Remove inactive threads
        QList <QFuture<void> > futures = m_jobThreads.futures();
        m_jobThreads.clearFutures();
        for (int i = 0; i < futures.count(); ++i)
            if (!futures.at(i).isFinished()) {
                m_jobThreads.addFuture(futures.at(i));
            }
    }
    if (m_jobList.isEmpty()) return;

    m_jobMutex.lock();
    int count = 0;
    for (int i = 0; i < m_jobList.count(); ++i) {
        if (m_jobList.at(i)->status() == JobWorking || m_jobList.at(i)->status() == JobWaiting)
            count ++;
        else {
            // remove finished jobs
            AbstractClipJob *job = m_jobList.takeAt(i);
            job->deleteLater();
            --i;
        }
    }
    emit jobCount(count);
    m_jobMutex.unlock();
    if (m_jobThreads.futures().isEmpty() || m_jobThreads.futures().count() < KdenliveSettings::proxythreads()) m_jobThreads.addFuture(QtConcurrent::run(this, &JobManager::slotProcessJobs));
}


void JobManager::slotProcessJobs()
{
    while (!m_jobList.isEmpty() && !m_abortAllJobs) {
        AbstractClipJob *job = NULL;
        int count = 0;
        m_jobMutex.lock();
        for (int i = 0; i < m_jobList.count(); ++i) {
            if (m_jobList.at(i)->status() == JobWaiting) {
                if (job == NULL) {
                    m_jobList.at(i)->setStatus(JobWorking);
                    job = m_jobList.at(i);
                }
                count++;
            }
            else if (m_jobList.at(i)->status() == JobWorking)
                count ++;
        }
        // Set jobs count
        emit jobCount(count);
        m_jobMutex.unlock();

        if (job == NULL) {
            break;
        }
        QString destination = job->destination();
        // Check if the clip is still here
        ProjectClip *currentClip = m_bin->getBinClip(job->clipId());
        //ProjectItem *processingItem = getItemById(job->clipId());
        if (currentClip == NULL) {
            job->setStatus(JobDone);
            continue;
        }
        // Set clip status to started
        currentClip->setJobStatus(job->jobType, job->status());

        // Make sure destination path is writable
        if (!destination.isEmpty()) {
            QFile file(destination);
            if (!file.open(QIODevice::WriteOnly)) {
                m_bin->updateJobStatus(job->clipId(), job->jobType, JobCrashed, i18n("Cannot write to path: %1", destination));
                job->setStatus(JobCrashed);
                continue;
            }
            file.close();
            QFile::remove(destination);
        }
        connect(job, SIGNAL(jobProgress(QString,int,int)), this, SIGNAL(processLog(QString,int,int)));
        connect(job, SIGNAL(cancelRunningJob(QString,stringMap)), this, SIGNAL(cancelRunningJob(QString,stringMap)));

        if (job->jobType == AbstractClipJob::MLTJOB) {
            MeltJob *jb = static_cast<MeltJob *> (job);
            if (jb->isProjectFilter())
                connect(job, SIGNAL(gotFilterJobResults(QString,int,int,stringMap,stringMap)), this, SLOT(slotGotFilterJobResults(QString,int,int,stringMap,stringMap)));
            else
                connect(job, SIGNAL(gotFilterJobResults(QString,int,int,stringMap,stringMap)), this, SIGNAL(gotFilterJobResults(QString,int,int,stringMap,stringMap)));
        }
        job->startJob();
        if (job->status() == JobDone) {
            m_bin->updateJobStatus(job->clipId(), job->jobType, JobDone);
            //TODO: replace with more generic clip replacement framework
            if (job->jobType == AbstractClipJob::PROXYJOB) {
                m_bin->gotProxy(job->clipId());
            }
            if (job->addClipToProject()) {
                emit addClip(destination, QString(), QString());
            }
        } else if (job->status() == JobCrashed || job->status() == JobAborted) {
            m_bin->updateJobStatus(job->clipId(), job->jobType, job->status(), job->errorMessage(), QString(), job->logDetails());
        }
    }
    // Thread finished, cleanup & update count
    QTimer::singleShot(200, this, SIGNAL(checkJobProcess()));
}

void JobManager::startJob(const QString &id, AbstractClipJob::JOBTYPE type, QStringList parameters)
{
    switch (type) {
      case AbstractClipJob::PROXYJOB:
	createProxy(id);
	break;
      default:
	break;
    }
}

//TODO: move to proxyclipjob
void JobManager::createProxy(const QString &id)
{
    ProjectClip *item = m_bin->getBinClip(id);
    if (!item || hasPendingJob(id, AbstractClipJob::PROXYJOB) /*|| item->referencedClip()->isPlaceHolder()*/) {
	return;
    }
    QString path = item->getProducerProperty("proxy");
    if (path.isEmpty()) {
        item->setJobStatus(AbstractClipJob::PROXYJOB, JobCrashed, -1, i18n("Failed to create proxy, empty path."));
        return;
    }
    
    if (QFileInfo(path).size() > 0) {
        // Proxy already created
        item->setJobStatus(AbstractClipJob::PROXYJOB, JobDone);
        m_bin->gotProxy(id);
        return;
    }
    QString sourcePath = item->url().toLocalFile();
    if (item->clipType() == Playlist) {
        // Special case: playlists use the special 'consumer' producer to support resizing
        sourcePath.prepend("consumer:");
    }
    QSize renderSize = m_bin->getRenderSize();
    ProxyJob *job = new ProxyJob(item->clipType(), id, QStringList() << path << sourcePath << item->getProducerProperty("_exif_orientation") << m_bin->getDocumentProperty("proxyparams").simplified() << QString::number(renderSize.width()) << QString::number(renderSize.height()));
    
    launchJob(item, job);
}

QList <ProjectClip *> JobManager::filterClips(QList <ProjectClip *>clips, AbstractClipJob::JOBTYPE jobType, const QStringList &params)
{
     //TODO: filter depending on clip type
    if (jobType == AbstractClipJob::TRANSCODEJOB) {
        return CutClipJob::filterClips(clips, params);
    }
    else if (jobType == AbstractClipJob::FILTERCLIPJOB) {
        return FilterJob::filterClips(clips, params);
    }
}

QStringList JobManager::prepareJobs(QList <ProjectClip *>clips, AbstractClipJob::JOBTYPE jobType, const QStringList params)
{
    //TODO filter clips
    //QMap <QString, QString> matching = filterClips(clips, jobType, params);

    QMap <ProjectClip *, AbstractClipJob *> jobs;
    if (jobType == AbstractClipJob::TRANSCODEJOB) {
        jobs = CutClipJob::prepareJob(m_fps, clips, params);
    } else if (jobType == AbstractClipJob::FILTERCLIPJOB) {
        jobs = FilterJob::prepareJob(m_fps, clips, params);
    }
    if (!jobs.isEmpty()) {
        QMapIterator<ProjectClip *, AbstractClipJob *> i(jobs);
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
    if (runQueue) slotCheckJobProcess();
}
