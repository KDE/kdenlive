/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "cutclipjob.h"
#include "kdenlivesettings.h"
#include "bin/projectclip.h"
#include "doc/kdenlivedoc.h"

#include "ui_cutjobdialog_ui.h"

#include <KMessageBox>
#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QPointer>
#include <klocalizedstring.h>

CutClipJob::CutClipJob(ClipType cType, const QString &id, const QStringList &parameters) : AbstractClipJob(CUTJOB, cType, id)
{
    m_jobStatus = JobWaiting;
    m_dest = parameters.at(0);
    m_src = parameters.at(1);
    m_start = parameters.at(2);
    m_end = parameters.at(3);
    if (m_start.isEmpty()) {
        // this is a transcoding job
        jobType = AbstractClipJob::TRANSCODEJOB;
        description = i18n("Transcode clip");
    } else {
        jobType = AbstractClipJob::CUTJOB;
        description = i18n("Cut clip");
    }
    m_jobDuration = parameters.at(4).toInt();
    m_addClipToProject = parameters.at(5).toInt();
    replaceClip = false;
    if (parameters.count() == 7) m_cutExtraParams = parameters.at(6).simplified();
}

void CutClipJob::startJob()
{
    // Special case: playlist clips (.mlt or .kdenlive project files)
    if (clipType == AV || clipType == Audio || clipType == Video) {
        QStringList parameters;
        parameters << QLatin1String("-i") << m_src;
        if (!m_start.isEmpty())
            parameters << QLatin1String("-ss") << m_start <<QLatin1String("-t") << m_end;
        if (!m_cutExtraParams.isEmpty()) {
            foreach(const QString &s, m_cutExtraParams.split(QLatin1Char(' ')))
                parameters << s;
        }

        // Make sure we don't block when proxy file already exists
        parameters << QLatin1String("-y");
        parameters << m_dest;
        qDebug()<<"/ / / STARTING CUT JOB: "<<parameters;
        m_jobProcess = new QProcess;
        m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), parameters);
        m_jobProcess->waitForStarted();
        while (m_jobProcess->state() != QProcess::NotRunning) {
            processLogInfo();
            if (m_jobStatus == JobAborted) {
                m_jobProcess->close();
                m_jobProcess->waitForFinished();
                QFile::remove(m_dest);
            }
            m_jobProcess->waitForFinished(400);
        }
        
        if (m_jobStatus != JobAborted) {
            int result = m_jobProcess->exitStatus();
            if (result == QProcess::NormalExit) {
                if (QFileInfo(m_dest).size() == 0) {
                    // File was not created
                    processLogInfo();
                    m_errorMessage.append(i18n("Failed to create file."));
                    setStatus(JobCrashed);
                } else {
                    setStatus(JobDone);
                }
            } else if (result == QProcess::CrashExit) {
                // Proxy process crashed
                QFile::remove(m_dest);
                setStatus(JobCrashed);
            }
        }
        delete m_jobProcess;
        return;
    } else {
        m_errorMessage = i18n("Cannot process this clip type.");
    }
    setStatus(JobCrashed);
    return;
}

void CutClipJob::processLogInfo()
{
    if (!m_jobProcess || m_jobDuration == 0 || m_jobStatus == JobAborted) return;
    QString log = QString::fromUtf8(m_jobProcess->readAll());
    if (!log.isEmpty()) m_logDetails.append(log + QLatin1Char('\n'));
    int progress;
    // Parse FFmpeg output
    //TODO: parsing progress info works with FFmpeg but not with libav
    if (log.contains(QLatin1String("frame="))) {
        progress = log.section(QLatin1String("frame="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
        emit jobProgress(m_clipId, (int) (100.0 * progress / m_jobDuration), jobType);
    }
    else if (log.contains(QLatin1String("time="))) {
        QString time = log.section(QLatin1String("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
        if (time.contains(QLatin1Char(':'))) {
            QStringList numbers = time.split(QLatin1Char(':'));
            progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
        }
        else progress = (int) time.toDouble();
        emit jobProgress(m_clipId, (int) (100.0 * progress / m_jobDuration), jobType);
    }
}

CutClipJob::~CutClipJob()
{
}

const QString CutClipJob::destination() const
{
    return m_dest;
}

stringMap CutClipJob::cancelProperties()
{
    QMap <QString, QString> props;
    return props;
}

const QString CutClipJob::statusMessage()
{
    QString statusInfo;
    switch (m_jobStatus) {
        case JobWorking:
            if (m_start.isEmpty()) statusInfo = i18n("Transcoding clip");
            else statusInfo = i18n("Extracting clip cut");
            break;
        case JobWaiting:
            if (m_start.isEmpty()) statusInfo = i18n("Waiting - transcode clip");
            else statusInfo = i18n("Waiting - cut clip");
            break;
        default:
            break;
    }
    return statusInfo;
}

bool CutClipJob::isExclusive()
{
    return false;
}

// static 
QList <ProjectClip *> CutClipJob::filterClips(QList <ProjectClip *>clips, const QStringList &params)
{
    QString condition;
    if (params.count() > 3) {
        // there is a condition for this job, for example operate only on vcodec=mpeg1video
        condition = params.at(3);
    }
    QList <ProjectClip *> result;
    for (int i = 0; i < clips.count(); i++) {
        ProjectClip *clip = clips.at(i);
        ClipType type = clip->clipType();
        if (type != AV && type != Audio && type != Video) {
            // Clip will not be processed by this job
            continue;
        }
        if (!condition.isEmpty() && !clip->matches(condition)) {
            // Clip does not match requested condition, do not process
            continue;
        }
        result << clip;
    }
    return result;
}

// static 

QMap <ProjectClip *, AbstractClipJob *> CutClipJob::prepareCutClipJob(double fps, double originalFps, ProjectClip *clip)
{
    QMap <ProjectClip *, AbstractClipJob *> jobs;
    if (!clip) return jobs;
    QString source = clip->url().toLocalFile();
    QPoint zone = clip->zone();
    QString ext = source.section('.', -1);
    QString dest = source.section('.', 0, -2) + '_' + QString::number(zone.x()) + '.' + ext;

    if (originalFps == 0) originalFps = fps;
    // if clip and project have different frame rate, adjust in and out
    int in = zone.x();
    int out = zone.y();
    in = GenTime(in, fps).frames(originalFps);
    out = GenTime(out, fps).frames(originalFps);
    int max = clip->duration().frames(originalFps);
    int duration = out - in + 1;
    QString timeIn = Timecode::getStringTimecode(in, originalFps, true);
    QString timeOut = Timecode::getStringTimecode(duration, originalFps, true);
    
    QPointer<QDialog> d = new QDialog(QApplication::activeWindow());
    Ui::CutJobDialog_UI ui;
    ui.setupUi(d);
    ui.extra_params->setVisible(false);
    ui.add_clip->setChecked(KdenliveSettings::add_new_clip());
    ui.file_url->setMode(KFile::File);
    ui.extra_params->setMaximumHeight(QFontMetrics(QApplication::font()).lineSpacing() * 5);
    ui.file_url->setUrl(QUrl(dest));
    ui.button_more->setIcon(QIcon::fromTheme("configure"));
    ui.extra_params->setPlainText("-acodec copy -vcodec copy");
    QString mess = i18n("Extracting %1 out of %2", timeOut, Timecode::getStringTimecode(max, originalFps, true));
    ui.info_label->setText(mess);
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return jobs;
    }
    dest = ui.file_url->url().path();
    bool acceptPath = dest != source;
    if (acceptPath && QFileInfo(dest).size() > 0) {
        // destination file olready exists, overwrite?
        acceptPath = false;
    }
    while (!acceptPath) {
        // Do not allow to save over original clip
        if (dest == source) ui.info_label->setText("<b>" + i18n("You cannot overwrite original clip.") + "</b><br>" + mess);
        else if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("Overwrite file %1", dest)) == KMessageBox::Yes) break;
        if (d->exec() != QDialog::Accepted) {
            delete d;
            return jobs;
        }
        dest = ui.file_url->url().path();
        acceptPath = dest != source;
        if (acceptPath && QFileInfo(dest).size() > 0) {
            acceptPath = false;
        }
    }
    QString extraParams = ui.extra_params->toPlainText().simplified();
    KdenliveSettings::setAdd_new_clip(ui.add_clip->isChecked());
    delete d;

    QStringList jobParams;
    jobParams << dest << source << timeIn << timeOut << QString::number(duration) << QString::number(KdenliveSettings::add_new_clip());
    if (!extraParams.isEmpty()) jobParams << extraParams;
    CutClipJob *job = new CutClipJob(clip->clipType(), clip->clipId(), jobParams);
    jobs.insert(clip, job);
    return jobs;
}

// static 
QMap <ProjectClip *, AbstractClipJob *> CutClipJob::prepareTranscodeJob(double fps, QList <ProjectClip*> clips, QStringList parameters)
{
    QMap <ProjectClip *, AbstractClipJob *> jobs;
    QString params = parameters.at(0);
    QString desc;
    if (parameters.count() > 1)
        desc = parameters.at(1);

    //const QString &condition, QString params, QString desc
    QStringList existingFiles;
    QStringList sources;
    QStringList destinations;
    for (int i = 0; i < clips.count(); i++) {
        QString source = clips.at(i)->url().toLocalFile();
        sources << source;
        QString newFile = params.section(' ', -1).replace("%1", source);
        destinations << newFile;
        if (QFile::exists(newFile)) existingFiles << newFile;
    }
    if (!existingFiles.isEmpty()) {
        if (KMessageBox::warningContinueCancelList(QApplication::activeWindow(), i18n("The transcoding job will overwrite the following files:"), existingFiles) ==  KMessageBox::Cancel) return jobs;
    }
    
    QPointer<QDialog> d = new QDialog(QApplication::activeWindow());
    Ui::CutJobDialog_UI ui;
    ui.setupUi(d);
    d->setWindowTitle(i18n("Transcoding"));
    ui.extra_params->setMaximumHeight(QFontMetrics(qApp->font()).lineSpacing() * 5);
    if (clips.count() == 1) {
        ui.file_url->setUrl(QUrl(destinations.first()));
    }
    else {
        ui.destination_label->setVisible(false);
        ui.file_url->setVisible(false);
    }
    ui.extra_params->setVisible(false);
    d->adjustSize();
    ui.button_more->setIcon(QIcon::fromTheme("configure"));
    connect(ui.button_more, SIGNAL(toggled(bool)), ui.extra_params, SLOT(setVisible(bool)));
    ui.add_clip->setChecked(KdenliveSettings::add_new_clip());
    ui.extra_params->setPlainText(params.simplified().section(' ', 0, -2));
    QString mess = desc;
    mess.append(' ' + i18np("(%1 clip)", "(%1 clips)", clips.count()));
    ui.info_label->setText(mess);
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return jobs;
    }
    params = ui.extra_params->toPlainText().simplified();
    KdenliveSettings::setAdd_new_clip(ui.add_clip->isChecked());
    for (int i = 0; i < clips.count(); i++) {
        ProjectClip *item = clips.at(i);
        QString src = sources.at(i);
        QString dest;
        if (clips.count() > 1) {
            dest = destinations.at(i);
        }
        else dest = ui.file_url->url().path();
        QStringList jobParams;
        jobParams << dest << src << QString() << QString();
        jobParams << QString::number((int) item->duration().frames(fps));
        jobParams << QString::number(KdenliveSettings::add_new_clip());
        jobParams << params;
        CutClipJob *job = new CutClipJob(item->clipType(), item->clipId(), jobParams);
        jobs.insert(item, job);
    }
    delete d;
    return jobs;
}
