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
#include "bin/projectclip.h"
#include "project/clipstabilize.h"
#include "meltjob.h"
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

void JobManager::slotStartFilterJob(const ItemInfo &info, const QString&id, QMap <QString, QString> &filterParams, QMap <QString, QString> &consumerParams, QMap <QString, QString> &extraParams)
{
    ProjectClip *item = m_bin->getBinClip(id);
    if (!item) return;

    QMap <QString, QString> producerParams = QMap <QString, QString> ();
    producerParams.insert("in", QString::number((int) info.cropStart.frames(m_fps)));
    producerParams.insert("out", QString::number((int) (info.cropStart + info.cropDuration).frames(m_fps)));
    extraParams.insert("clipStartPos", QString::number((int) info.startPos.frames(m_fps)));
    extraParams.insert("clipTrack", QString::number(info.track));

    MeltJob *job = new MeltJob(item->clipType(), id, producerParams, filterParams, consumerParams, extraParams);
    if (job->isExclusive() && hasPendingJob(id, job->jobType)) {
        delete job;
        return;
    }
    job->description = i18n("Filter %1", extraParams.value("finalfilter"));
    m_jobList.append(job);
    item->setJobStatus(job->jobType, JobWaiting, 0, job->statusMessage());
    slotCheckJobProcess();
}

void JobManager::startClipFilterJob(QList <ProjectClip *> clipList, const QString &filterName)
{
    QStringList destination;
    QStringList ids;
    for (int i = 0; i < clipList.count(); i++) {
	ProjectClip *item = clipList.at(i);
	if (!item) {
	    emit displayMessage(i18n("Cannot find clip to process filter %1", filterName), -2, ErrorMessage);
	    continue;
	}
	else {
	    ids << item->clipId();
	    destination << item->url().toLocalFile();
	}
    }
    if (filterName == "framebuffer") {
        Mlt::Profile profile;
        int ix = 0;
        foreach(const QString &url, destination) {
            QString prodstring = QString("framebuffer:" + url + "?-1");
            Mlt::Producer *reversed = new Mlt::Producer(profile, prodstring.toUtf8().constData());
            if (!reversed || !reversed->is_valid()) {
                emit displayMessage(i18n("Cannot reverse clip"), -2, ErrorMessage);
                continue;
            }
            QString dest = url + ".mlt";
            if (QFile::exists(dest)) {
                if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("File %1 already exists.\nDo you want to overwrite it?", dest)) == KMessageBox::No) continue;
            }
            Mlt::Consumer *cons = new Mlt::Consumer(profile, "xml", dest.toUtf8().constData());
            if (cons == NULL || !cons->is_valid()) {
                emit displayMessage(i18n("Cannot render reversed clip"), -2, ErrorMessage);
                continue;
            }
            Mlt::Playlist list;
            list.insert_at(0, reversed, 0);
            delete reversed;
            cons->connect(list);
            cons->run();
            delete cons;
            QString groupId;
            QString groupName;
            //TODO: get clip folder
            /*if (base) {
                groupId = base->getProperty("groupid");
                groupName = base->getProperty("groupname");
            }*/
            emit addClip(dest, groupId, groupName);
            ix++;
        }
        return;
    }
    
    if (filterName == "motion_est") {
        // Show config dialog
        QPointer<QDialog> d = new QDialog(QApplication::activeWindow());
        Ui::SceneCutDialog_UI ui;
        ui.setupUi(d);
        // Set  up categories
        for (int i = 0; i < 5; ++i) {
            ui.marker_type->insertItem(i, i18n("Category %1", i));
            ui.marker_type->setItemData(i, CommentedTime::markerColor(i), Qt::DecorationRole);
        }
        ui.marker_type->setCurrentIndex(KdenliveSettings::default_marker_type());
        if (d->exec() != QDialog::Accepted) {
            delete d;
            return;
        }
        // Autosplit filter
        QMap <QString, QString> producerParams = QMap <QString, QString> ();
        QMap <QString, QString> filterParams = QMap <QString, QString> ();
	QMap <QString, QString> consumerParams = QMap <QString, QString> ();
	
        // Producer params
	// None

        // Filter params, use a smaller region of the image to speed up operation
        // In fact, it's faster to rescale whole image than using part of it (bounding=\"25%x25%:15%x15\")
	filterParams.insert("filter", filterName);
	filterParams.insert("shot_change_list", "0");
	filterParams.insert("denoise", "0");
	
	// Consumer
	consumerParams.insert("consumer", "null");
	consumerParams.insert("all", "1");
	consumerParams.insert("terminate_on_pause", "1");
	consumerParams.insert("real_time", "-1");
	// We just want to find scene change, set all mathods to the fastests
	consumerParams.insert("rescale", "nearest");
	consumerParams.insert("deinterlace_method", "onefield");
	consumerParams.insert("top_field_first", "-1");
	
        // Extra
        QMap <QString, QString> extraParams;
        extraParams.insert("key", "shot_change_list");
        extraParams.insert("projecttreefilter", "1");
        QString keyword("%count");
        extraParams.insert("resultmessage", i18n("Found %1 scenes.", keyword));
        extraParams.insert("resize_profile", "160");
        if (ui.store_data->isChecked()) {
            // We want to save result as clip metadata
            extraParams.insert("storedata", "1");
        }
        if (ui.zone_only->isChecked()) {
            // We want to analyze only clip zone
            extraParams.insert("zoneonly", "1");
        }
        if (ui.add_markers->isChecked()) {
            // We want to create markers
            extraParams.insert("addmarkers", QString::number(ui.marker_type->currentIndex()));
        }
        if (ui.cut_scenes->isChecked()) {
            // We want to cut scenes
            extraParams.insert("cutscenes", "1");
        }
        delete d;
        processClipJob(ids, QString(), false, producerParams, filterParams, consumerParams, i18n("Auto split"), extraParams);
    }
    else {
        QPointer<ClipStabilize> d = new ClipStabilize(destination, filterName);
        if (d->exec() == QDialog::Accepted) {
            QMap <QString, QString> extraParams;
            extraParams.insert("producer_profile", "1");
            processClipJob(ids, d->destination(), d->autoAddClip(), d->producerParams(), d->filterParams(), d->consumerParams(), d->desc(), extraParams);
        }
        delete d;
    }
}

void JobManager::processClipJob(QStringList ids, const QString&destination, bool autoAdd, QMap <QString, QString> producerParams, QMap <QString, QString> filterParams, QMap <QString, QString> consumerParams, const QString &description, QMap <QString, QString> extraParams)
{
    // in and out
    int in = 0;
    int out = -1;
 
    // filter name
    QString filterName = filterParams.value("filter");

    // consumer name
    QString consumerName = consumerParams.value("consumer");

    foreach(const QString&id, ids) {
        ProjectClip *item = m_bin->getBinClip(id);
        if (!item) continue;
	//TODO manage bin clip zones
	/*
        if (extraParams.contains("zoneonly")) {
            // Analyse clip zone only, remove in / out and replace with zone
            QPoint zone = item->zone();
	    in = zone.x();
	    out = zone.y();
        }*/
        producerParams.insert("in", QString::number(in));
	producerParams.insert("out", QString::number(out));

        if (ids.count() == 1) {
	    // We only have one clip to process
	    if (filterName == "vidstab") {
		// Append a 'filename' parameter for saving vidstab data
		QUrl trffile = QUrl::fromLocalFile(destination + ".trf");
		filterParams.insert("filename", trffile.path());
		consumerParams.insert("consumer", consumerName + ':' + destination);
           } else {
		consumerParams.insert("consumer", consumerName + ':' + destination);
           }
        }
        else {
	    // We have several clips to process
	    QString mltfile = destination + item->url().fileName() + ".mlt";
            if (filterName == "vidstab") {
		// Append a 'filename' parameter for saving each vidstab data
		QUrl trffile = QUrl::fromLocalFile(mltfile + ".trf");
		filterParams.insert("filename", trffile.path());
		consumerParams.insert("consumer", consumerName + ':' + mltfile);
            } else {
		consumerParams.insert("consumer", consumerName + ':' + mltfile);
            }
        }
        MeltJob *job = new MeltJob(item->clipType(), id, producerParams, filterParams, consumerParams, extraParams);
        if (autoAdd) {
            job->setAddClipToProject(true);
            //qDebug()<<"// ADDING TRUE";
        }
        else qDebug()<<"// ADDING FALSE!!!";

        if (job->isExclusive() && hasPendingJob(id, job->jobType)) {
            delete job;
            return;
        }
        job->description = description;
        m_jobList.append(job);
	item->setJobStatus(job->jobType, JobWaiting, 0, job->statusMessage());
        slotCheckJobProcess();
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
            //jb->setProducer(currentClip->getProducer(), currentClip->fileURL());
	    jb->setProducer(currentClip->producer(), currentClip->url());
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

void JobManager::startJob(const QString &id, AbstractClipJob::JOBTYPE type)
{
    switch (type) {
      case AbstractClipJob::PROXYJOB:
	createProxy(id);
	break;
      default:
	break;
    }
}

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
    if (job->isExclusive() && hasPendingJob(id, job->jobType)) {
        delete job;
        return;
    }

    m_jobList.append(job);
    item->setJobStatus(job->jobType, JobWaiting, 0, job->statusMessage());
    slotCheckJobProcess();
}
