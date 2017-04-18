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
#include <klocalizedstring.h>

#include <QApplication>
#include "kdenlive_debug.h"
#include <QDialog>
#include <QPointer>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

CutClipJob::CutClipJob(ClipType cType, const QString &id, const QStringList &parameters) : AbstractClipJob(CUTJOB, cType, id)
{
    m_jobStatus = JobWaiting;
    jobType = (AbstractClipJob::JOBTYPE) parameters.at(0).toInt();
    m_dest = parameters.at(1);
    m_src = parameters.at(2);
    switch (jobType) {
    case AbstractClipJob::TRANSCODEJOB:
        description = i18n("Transcode clip");
        break;
    case AbstractClipJob::CUTJOB:
        description = i18n("Cut clip");
        break;
    case AbstractClipJob::ANALYSECLIPJOB:
    default:
        description = i18n("Analyse clip");
        break;
    }
    replaceClip = false;
    if (jobType != AbstractClipJob::ANALYSECLIPJOB) {
        m_start = parameters.at(3);
        m_end = parameters.at(4);
        m_jobDuration = parameters.at(5).toInt();
        m_addClipToProject = parameters.at(6).toInt();
        if (parameters.count() == 8) {
            m_cutExtraParams = parameters.at(7).simplified();
        }
    } else {
        m_jobDuration = parameters.at(3).toInt();
    }
}

void CutClipJob::startJob()
{
    // Special case: playlist clips (.mlt or .kdenlive project files)
    if (clipType == AV || clipType == Audio || clipType == Video) {
        if (KdenliveSettings::ffmpegpath().isEmpty()) {
            //FFmpeg not detected, cannot process the Job
            m_errorMessage = i18n("Cannot process job. FFmpeg not found, please set path in Kdenlive's settings Environment");
            setStatus(JobCrashed);
            return;
        }
        QStringList parameters;
        QString exec;
        if (jobType == AbstractClipJob::ANALYSECLIPJOB) {
            // TODO: don't hardcode params
            parameters << QStringLiteral("-select_streams") << QStringLiteral("v") << QStringLiteral("-show_frames") << QStringLiteral("-hide_banner") << QStringLiteral("-of") << QStringLiteral("json=c=1") << m_src;
            exec = KdenliveSettings::ffprobepath();
        } else {
            if (!m_cutExtraParams.contains(QLatin1String("-i "))) {
                parameters << QStringLiteral("-i") << m_src;
            }
            if (!m_start.isEmpty()) {
                parameters << QStringLiteral("-ss") << m_start << QStringLiteral("-t") << m_end;
            }
            if (!m_cutExtraParams.isEmpty()) {
                foreach (const QString &s, m_cutExtraParams.split(QLatin1Char(' '))) {
                    parameters << s;
                    if (s == QLatin1String("-i")) {
                        parameters << m_src;
                    }
                }
            }

            // Make sure we don't block when proxy file already exists
            parameters << QStringLiteral("-y");
            parameters << m_dest;
            exec = KdenliveSettings::ffmpegpath();
        }
        m_jobProcess = new QProcess;
        if (jobType != AbstractClipJob::ANALYSECLIPJOB) {
            m_jobProcess->setProcessChannelMode(QProcess::MergedChannels);
        }
        m_jobProcess->start(exec, parameters);
        m_jobProcess->waitForStarted();
        while (m_jobProcess->state() != QProcess::NotRunning) {
            if (jobType == AbstractClipJob::ANALYSECLIPJOB) {
                analyseLogInfo();
            } else {
                processLogInfo();
            }
            if (m_jobStatus == JobAborted) {
                m_jobProcess->close();
                m_jobProcess->waitForFinished();
                if (!m_dest.isEmpty()) {
                    QFile::remove(m_dest);
                }
            }
            m_jobProcess->waitForFinished(400);
        }

        if (m_jobStatus != JobAborted) {
            int result = m_jobProcess->exitStatus();
            if (result == QProcess::NormalExit) {
                if (jobType == AbstractClipJob::ANALYSECLIPJOB) {
                    analyseLogInfo();
                    processAnalyseLog();
                    setStatus(JobDone);
                } else {
                    if (QFileInfo(m_dest).size() == 0) {
                        // File was not created
                        processLogInfo();
                        m_errorMessage.append(i18n("Failed to create file."));
                        setStatus(JobCrashed);
                    } else {
                        setStatus(JobDone);
                    }
                }
            } else if (result == QProcess::CrashExit) {
                // Proxy process crashed
                if (!m_dest.isEmpty()) {
                    QFile::remove(m_dest);
                }
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
    if (!m_jobProcess || m_jobDuration == 0 || m_jobStatus == JobAborted) {
        return;
    }
    QString log = QString::fromUtf8(m_jobProcess->readAll());
    if (!log.isEmpty()) {
        m_logDetails.append(log + QLatin1Char('\n'));
    }
    int progress;
    // Parse FFmpeg output
    //TODO: parsing progress info works with FFmpeg but not with libav
    if (log.contains(QLatin1String("frame="))) {
        progress = log.section(QStringLiteral("frame="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0).toInt();
        emit jobProgress(m_clipId, (int)(100.0 * progress / m_jobDuration), jobType);
    } else if (log.contains(QLatin1String("time="))) {
        QString time = log.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
        if (time.contains(QLatin1Char(':'))) {
            QStringList numbers = time.split(QLatin1Char(':'));
            progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toDouble();
        } else {
            progress = (int) time.toDouble();
        }
        emit jobProgress(m_clipId, (int)(100.0 * progress / m_jobDuration), jobType);
    }
}

void CutClipJob::analyseLogInfo()
{
    if (!m_jobProcess || m_jobStatus == JobAborted) {
        return;
    }
    QString log = QString::fromUtf8(m_jobProcess->readAll());
    m_logDetails.append(log);
    int pos = log.indexOf(QStringLiteral("coded_picture_number"), 0);
    if (pos > -1) {
        log.remove(0, pos);
        int frame = log.section(QLatin1Char(','), 0, 0).section(QLatin1Char(':'), 1).toInt();
        if (frame > 0 && m_jobDuration > 0) {
            emit jobProgress(m_clipId, (int)(100.0 * frame / m_jobDuration), jobType);
        }
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
    QMap<QString, QString> props;
    return props;
}

const QString CutClipJob::statusMessage()
{
    QString statusInfo;
    switch (m_jobStatus) {
    case JobWorking:
        if (jobType == AbstractClipJob::TRANSCODEJOB) {
            statusInfo = i18n("Transcoding clip");
        } else if (jobType == AbstractClipJob::CUTJOB) {
            statusInfo = i18n("Extracting clip cut");
        } else {
            statusInfo = i18n("Analysing clip");
        }
        break;
    case JobWaiting:
        if (jobType == AbstractClipJob::TRANSCODEJOB) {
            statusInfo = i18n("Waiting - transcode clip");
        } else if (jobType == AbstractClipJob::CUTJOB) {
            statusInfo = i18n("Waiting - cut clip");
        } else {
            statusInfo = i18n("Waiting - analyse clip");
        }
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
QList<ProjectClip *> CutClipJob::filterClips(const QList<ProjectClip *> &clips, const QStringList &params)
{
    QString condition;
    if (params.count() > 3) {
        // there is a condition for this job, for example operate only on vcodec=mpeg1video
        condition = params.at(3);
    }
    QList<ProjectClip *> result;
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

QHash<ProjectClip *, AbstractClipJob *> CutClipJob::prepareCutClipJob(double fps, double originalFps, ProjectClip *clip)
{
    QHash<ProjectClip *, AbstractClipJob *> jobs;
    if (!clip) {
        return jobs;
    }
    QString source = clip->url();
    QPoint zone = clip->zone();
    QString ext = source.section(QLatin1Char('.'), -1);
    QString dest = source.section(QLatin1Char('.'), 0, -2) + QLatin1Char('_') + QString::number(zone.x()) + QLatin1Char('.') + ext;

    if (originalFps < 1e-6) {
        originalFps = fps;
    }
    // if clip and project have different frame rate, adjust in and out
    int in = zone.x();
    int out = zone.y();
    int max = clip->duration().frames(originalFps);
    int duration = out - in - 1;
    // Locale conversion does not seem necessary here...
    QString timeIn = QString::number(GenTime(in, originalFps).ms() / 1000);
    QString timeOut = QString::number(GenTime(duration, originalFps).ms() / 1000);

    QPointer<QDialog> d = new QDialog(QApplication::activeWindow());
    Ui::CutJobDialog_UI ui;
    ui.setupUi(d);
    ui.extra_params->setVisible(false);
    ui.add_clip->setChecked(KdenliveSettings::add_new_clip());
    ui.file_url->setMode(KFile::File);
    ui.extra_params->setMaximumHeight(QFontMetrics(QApplication::font()).lineSpacing() * 5);
    ui.file_url->setUrl(QUrl(dest));
    ui.button_more->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    ui.extra_params->setPlainText(QStringLiteral("-acodec copy -vcodec copy"));
    QString mess = i18n("Extracting %1 out of %2", Timecode::getStringTimecode(duration, fps, true), Timecode::getStringTimecode(max, fps, true));
    ui.info_label->setText(mess);
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return jobs;
    }
    dest = ui.file_url->url().toLocalFile();
    bool acceptPath = dest != source;
    if (acceptPath && QFileInfo(dest).size() > 0) {
        // destination file olready exists, overwrite?
        acceptPath = false;
    }
    while (!acceptPath) {
        // Do not allow to save over original clip
        if (dest == source) {
            ui.info_label->setText(QStringLiteral("<b>") + i18n("You cannot overwrite original clip.") + QStringLiteral("</b><br>") + mess);
        } else if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("Overwrite file %1", dest)) == KMessageBox::Yes) {
            break;
        }
        if (d->exec() != QDialog::Accepted) {
            delete d;
            return jobs;
        }
        dest = ui.file_url->url().toLocalFile();
        acceptPath = dest != source;
        if (acceptPath && QFileInfo(dest).size() > 0) {
            acceptPath = false;
        }
    }
    QString extraParams = ui.extra_params->toPlainText().simplified();
    KdenliveSettings::setAdd_new_clip(ui.add_clip->isChecked());
    delete d;

    QStringList jobParams;
    jobParams << QString::number((int) AbstractClipJob::CUTJOB);
    jobParams << dest << source << timeIn << timeOut << QString::number(duration);
    // parent folder, or -100 if we don't want to add clip to project
    jobParams << (KdenliveSettings::add_new_clip() ? clip->parent()->clipId() : QString::number(-100));
    if (!extraParams.isEmpty()) {
        jobParams << extraParams;
    }
    CutClipJob *job = new CutClipJob(clip->clipType(), clip->clipId(), jobParams);
    jobs.insert(clip, job);
    return jobs;
}

// static
QHash<ProjectClip *, AbstractClipJob *> CutClipJob::prepareTranscodeJob(double fps, const QList<ProjectClip *> &clips, const QStringList &parameters)
{
    QHash<ProjectClip *, AbstractClipJob *> jobs;
    QString params = parameters.at(0);
    QString desc;
    if (parameters.count() > 1) {
        desc = parameters.at(1);
    }

    QStringList existingFiles;
    QStringList sources;
    QStringList destinations;
    destinations.reserve(clips.count());
    sources.reserve(clips.count());
    for (int i = 0; i < clips.count(); i++) {
        QString source = clips.at(i)->url();
        sources << source;
        QString newFile = params.section(QLatin1Char(' '), -1).replace(QLatin1String("%1"), source);
        destinations << newFile;
        if (QFile::exists(newFile)) {
            existingFiles << newFile;
        }
    }
    if (!existingFiles.isEmpty()) {
        if (KMessageBox::warningContinueCancelList(QApplication::activeWindow(), i18n("The transcoding job will overwrite the following files:"), existingFiles) ==  KMessageBox::Cancel) {
            return jobs;
        }
    }

    QPointer<QDialog> d = new QDialog(QApplication::activeWindow());
    Ui::CutJobDialog_UI ui;
    ui.setupUi(d);
    d->setWindowTitle(i18n("Transcoding"));
    ui.extra_params->setMaximumHeight(QFontMetrics(qApp->font()).lineSpacing() * 5);
    if (clips.count() == 1) {
        ui.file_url->setUrl(QUrl(destinations.first()));
    } else {
        ui.destination_label->setVisible(false);
        ui.file_url->setVisible(false);
    }
    ui.extra_params->setVisible(false);
    d->adjustSize();
    ui.button_more->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    connect(ui.button_more, &QAbstractButton::toggled, ui.extra_params, &QWidget::setVisible);
    ui.add_clip->setChecked(KdenliveSettings::add_new_clip());
    ui.extra_params->setPlainText(params.simplified().section(QLatin1Char(' '), 0, -2));
    QString mess = desc;
    mess.append(QLatin1Char(' ') + i18np("(%1 clip)", "(%1 clips)", clips.count()));
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
        } else {
            dest = ui.file_url->url().toLocalFile();
        }
        QStringList jobParams;
        jobParams << QString::number((int) AbstractClipJob::TRANSCODEJOB);
        jobParams << dest << src << QString() << QString();
        jobParams << QString::number((int) item->duration().frames(fps));
        // parent folder, or -100 if we don't want to add clip to project
        jobParams << (KdenliveSettings::add_new_clip() ? item->parent()->clipId() : QString::number(-100));
        jobParams << params;
        CutClipJob *job = new CutClipJob(item->clipType(), item->clipId(), jobParams);
        jobs.insert(item, job);
    }
    delete d;
    return jobs;
}

// static
QHash<ProjectClip *, AbstractClipJob *> CutClipJob::prepareAnalyseJob(double fps, const QList<ProjectClip *> &clips, const QStringList &parameters)
{
    // Might be useful some day
    Q_UNUSED(parameters);

    QHash<ProjectClip *, AbstractClipJob *> jobs;
    for (ProjectClip *clip : clips) {
        QString source = clip->url();
        QStringList jobParams;
        int duration = clip->duration().frames(fps) * clip->getOriginalFps() / fps;
        jobParams << QString::number((int) AbstractClipJob::ANALYSECLIPJOB) << QString() << source << QString::number(duration);
        CutClipJob *job = new CutClipJob(clip->clipType(), clip->clipId(), jobParams);
        jobs.insert(clip, job);
    }
    return jobs;
}

void CutClipJob::processAnalyseLog()
{
    QJsonDocument doc = QJsonDocument::fromJson(m_logDetails.toUtf8());
    if (doc.isEmpty()) {
        qCDebug(KDENLIVE_LOG) << "+ + + + +CORRUPTED JSON DOC";
    }
    QJsonObject jsonObject = doc.object();
    const QJsonArray jsonArray = jsonObject[QStringLiteral("frames")].toArray();
    QList<int> frames;
    for (const QJsonValue &value : jsonArray) {
        QJsonObject obj = value.toObject();
        if (obj[QStringLiteral("pict_type")].toString() != QLatin1String("I")) {
            continue;
        }
        frames << obj[QStringLiteral("coded_picture_number")].toInt();
    }
    qSort(frames);
    QMap<QString, QString> jobResults;
    QStringList sortedFrames;
    foreach (int frm, frames) {
        sortedFrames << QString::number(frm);
    }
    jobResults.insert(QStringLiteral("i-frame"), sortedFrames.join(QLatin1Char(';')));
    QMap<QString, QString> extraInfo;
    extraInfo.insert(QStringLiteral("addmarkers"), QStringLiteral("3"));
    extraInfo.insert(QStringLiteral("key"), QStringLiteral("i-frame"));
    extraInfo.insert(QStringLiteral("simplelist"), QStringLiteral("1"));
    extraInfo.insert(QStringLiteral("label"), i18n("I-Frame "));
    extraInfo.insert(QStringLiteral("resultmessage"), i18n("Found %count I-Frames"));
    emit gotFilterJobResults(m_clipId, 0, 0, jobResults, extraInfo);
}
