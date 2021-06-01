/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "cuttask.h"
#include "bin/bin.h"
#include "mainwindow.h"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "ui_cutjobdialog_ui.h"
#include "bin/projectitemmodel.h"
#include "profiles/profilemodel.hpp"
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "xml/xml.hpp"

#include <QThread>
#include <QProcess>
#include <KIO/RenameDialog>
#include <KLineEdit>
#include <QComboBox>
#include <QObject>
#include <klocalizedstring.h>

CutTask::CutTask(const ObjectId &owner, const QString &destination, const QStringList encodingParams, int in, int out, bool addToProject, QObject* object)
    : AbstractTask(owner, AbstractTask::CUTJOB, object)
    , m_inPoint(GenTime(in, pCore->getCurrentFps()))
    , m_outPoint(GenTime(out, pCore->getCurrentFps()))
    , m_destination(destination)
    , m_encodingParams(encodingParams)
    , m_jobDuration(0)
    , m_addToProject(addToProject)
{
}

void CutTask::start(const ObjectId &owner, int in , int out, QObject* object, bool force)
{
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(owner.second));
    ClipType::ProducerType type = binClip->clipType();
    if (type != ClipType::AV && type != ClipType::Audio && type != ClipType::Video) {
        //m_errorMessage.prepend(i18n("Cannot extract zone for this clip type."));
        return;
    }
    const QString source = binClip->url();
    QString videoCodec = binClip->codec(false);
    QString audioCodec = binClip->codec(true);
    // Check if the audio/video codecs are supported for encoding (required for the codec copy feature)
    QProcess checkProcess;
    QStringList params = {QStringLiteral("-codecs")};
    checkProcess.start(KdenliveSettings::ffmpegpath(), params);
    checkProcess.waitForFinished(); // sets current thread to sleep and waits for pingProcess end
    QString output(checkProcess.readAllStandardOutput());
    QString line;
    QTextStream stream(&output);
    bool videoOk = videoCodec.isEmpty();
    bool audioOk = audioCodec.isEmpty();
    while (stream.readLineInto(&line)) {
        if (!videoOk && line.contains(videoCodec)) {
            if (line.simplified().section(QLatin1Char(' '), 0, 0).contains(QLatin1Char('E'))) {
                videoOk = true;
            }
        } else if (!audioOk && line.contains(audioCodec)) {
            if (line.simplified().section(QLatin1Char(' '), 0, 0).contains(QLatin1Char('E'))) {
                audioOk = true;
            }
        }
        if (audioOk && videoOk) {
            break;
        }
    }
    QString warnMessage;
    if (!videoOk) {
        warnMessage = i18n("Cannot copy video codec %1, will re-encode.", videoCodec);
    }
    if (!audioOk) {
        if (!videoOk) {
            warnMessage.append(QLatin1Char('\n'));
        }
        warnMessage.append(i18n("Cannot copy audio codec %1, will re-encode.", audioCodec));
    }
    QString transcoderExt = source.section(QLatin1Char('.'), -1);
    transcoderExt.prepend(QLatin1Char('.'));
    QFileInfo finfo(source);
    QString fileName = finfo.fileName().section(QLatin1Char('.'), 0, -2);
    QDir dir = finfo.absoluteDir();
    QString inString = QString::number(int(GenTime(in, pCore->getCurrentFps()).seconds()));
    QString outString = QString::number(int(GenTime(out, pCore->getCurrentFps()).seconds()));
    QString path = dir.absoluteFilePath(fileName + QString("-%1-%2").arg(inString, outString) + transcoderExt);

    QPointer<QDialog> d = new QDialog(QApplication::activeWindow());
    Ui::CutJobDialog_UI ui;
    ui.setupUi(d);
    ui.extra_params->setVisible(false);
    ui.message->setText(warnMessage);
    ui.message->setVisible(!warnMessage.isEmpty());
    if (videoCodec.isEmpty()) {
        ui.video_codec->setText(i18n("none"));
        ui.vcodec->setEnabled(false);
    } else {
        ui.video_codec->setText(videoCodec);
        ui.vcodec->addItem(i18n("Copy stream"), QStringLiteral("copy"));
        ui.vcodec->addItem(i18n("X264 encoding"), QStringLiteral("libx264"));
    }
    if (audioCodec.isEmpty()) {
        ui.audio_codec->setText(i18n("none"));
        ui.acodec->setEnabled(false);
    } else {
        ui.audio_codec->setText(audioCodec);
        ui.acodec->addItem(i18n("Copy stream"), QStringLiteral("copy"));
        ui.acodec->addItem(i18n("PCM encoding"), QStringLiteral("pcm_s24le"));
        ui.acodec->addItem(i18n("AAC encoding"), QStringLiteral("aac"));
    }
    ui.audio_codec->setText(audioCodec);
    ui.add_clip->setChecked(KdenliveSettings::add_new_clip());
    ui.file_url->setMode(KFile::File);
    ui.extra_params->setMaximumHeight(QFontMetrics(QApplication::font()).lineSpacing() * 5);
    ui.file_url->setUrl(QUrl::fromLocalFile(path));
    QObject::connect(ui.acodec, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), d.data(), [&ui, transcoderExt]() {
        ui.extra_params->setPlainText(QString("-c:a %1 -c:v %2").arg(ui.acodec->currentData().toString()).arg(ui.vcodec->currentData().toString()));
        QString path = ui.file_url->url().toLocalFile();
        QString fileName = path.section(QLatin1Char('.'), 0, -2);
        if (ui.acodec->currentData() == QLatin1String("copy") && ui.vcodec->currentData() == QLatin1String("copy")) {
            fileName.append(transcoderExt);
        } else {
            fileName.append(QStringLiteral(".mov"));
        }
        ui.file_url->setUrl(QUrl::fromLocalFile(fileName));
    });
    QObject::connect(ui.vcodec, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), d.data(), [&ui, transcoderExt]() {
        ui.extra_params->setPlainText(QString("-c:a %1 -c:v %2").arg(ui.acodec->currentData().toString()).arg(ui.vcodec->currentData().toString()));
        QString path = ui.file_url->url().toLocalFile();
        QString fileName = path.section(QLatin1Char('.'), 0, -2);
        if (ui.acodec->currentData() == QLatin1String("copy") && ui.vcodec->currentData() == QLatin1String("copy")) {
            fileName.append(transcoderExt);
        } else {
            fileName.append(QStringLiteral(".mov"));
        }
        ui.file_url->setUrl(QUrl::fromLocalFile(fileName));
    });
    QFontMetrics fm = ui.file_url->lineEdit()->fontMetrics();
    ui.file_url->setMinimumWidth(int(fm.boundingRect(ui.file_url->text().left(50)).width() * 1.4));
    ui.button_more->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    ui.extra_params->setPlainText(QStringLiteral("-c:a copy -c:v copy"));
    QString mess = i18n("Extracting %1 out of %2", Timecode::getStringTimecode(out - in, pCore->getCurrentFps(), true), binClip->getStringDuration());
    ui.info_label->setText(mess);
    if (!videoOk) {
        ui.vcodec->setCurrentIndex(1);
    }
    if (!audioOk) {
        ui.acodec->setCurrentIndex(1);
    }
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return;
    }
    path = ui.file_url->url().toLocalFile();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList encodingParams = ui.extra_params->toPlainText().split(QLatin1Char(' '), QString::SkipEmptyParts);
#else
    QStringList encodingParams = ui.extra_params->toPlainText().split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
    KdenliveSettings::setAdd_new_clip(ui.add_clip->isChecked());
    delete d;

    if (QFile::exists(path)) {
        KIO::RenameDialog renameDialog(qApp->activeWindow(), i18n("File already exists"), QUrl::fromLocalFile(path), QUrl::fromLocalFile(path), KIO::RenameDialog_Option::RenameDialog_Overwrite );
        if (renameDialog.exec() != QDialog::Rejected) {
            QUrl final = renameDialog.newDestUrl();
            if (final.isValid()) {
                path = final.toLocalFile();
            }
        } else {
            return;
        }
    }
    CutTask *task = new CutTask(owner, path, encodingParams, in, out, ui.add_clip->isChecked(), object);
    if (task) {
        // Otherwise, start a filter thread.
        task->m_isForce = force;
        pCore->taskManager.startTask(owner.second, task);
    }
}

void CutTask::run()
{
    if (m_isCanceled) {
        pCore->taskManager.taskDone(m_owner.second, this);
        return;
    }
    m_running = true;
    qDebug()<<" + + + + + + + + STARTING STAB TASK";
    
    QString url;
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.second));
    if (binClip) {
        // Filter applied on a timeline or bin clip
        url = binClip->url();
        QString folder = QStringLiteral("-1");
        auto containingFolder = std::static_pointer_cast<ProjectFolder>(binClip->parent());
        if (containingFolder) {
            folder = containingFolder->clipId();
        }
        if (url.isEmpty()) {
            m_errorMessage.append(i18n("No producer for this clip."));
            pCore->taskManager.taskDone(m_owner.second, this);
            return;
        }
        if (QFileInfo(m_destination).absoluteFilePath() == QFileInfo(url).absoluteFilePath()) {
            m_errorMessage.append(i18n("You cannot overwrite original clip."));
            pCore->taskManager.taskDone(m_owner.second, this);
            return;
        }
        QStringList params = {QStringLiteral("-y"),QStringLiteral("-stats"),QStringLiteral("-v"),QStringLiteral("error"),QStringLiteral("-noaccurate_seek"),QStringLiteral("-ss"),QString::number(m_inPoint.seconds()),QStringLiteral("-i"),url, QStringLiteral("-t"), QString::number((m_outPoint-m_inPoint).seconds()),QStringLiteral("-avoid_negative_ts"),QStringLiteral("make_zero"),QStringLiteral("-sn"),QStringLiteral("-dn"),QStringLiteral("-map"),QStringLiteral("0")};
        params << m_encodingParams << m_destination;
        m_jobProcess = std::make_unique<QProcess>(new QProcess);
        connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &CutTask::processLogInfo);
        connect(this, &CutTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
        qDebug()<<"=== STARTING CUT JOB: "<<params;
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), params, QIODevice::ReadOnly);
        m_jobProcess->waitForFinished(-1);
        bool result = m_jobProcess->exitStatus() == QProcess::NormalExit;
        // remove temporary playlist if it exists
        if (result && !m_isCanceled) {
            if (QFileInfo(m_destination).size() == 0) {
                QFile::remove(m_destination);
                // File was not created
                m_errorMessage.append(i18n("Failed to create file."));
            } else {
                // all ok, add clip
                if (m_addToProject) {
                    QMetaObject::invokeMethod(pCore->window(), "addProjectClip", Qt::QueuedConnection, Q_ARG(const QString&,m_destination), Q_ARG(const QString&,folder));
                }
            }
        } else {
            // Proxy process crashed
            QFile::remove(m_destination);
            m_errorMessage.append(QString::fromUtf8(m_jobProcess->readAll()));
        }
    }
    pCore->taskManager.taskDone(m_owner.second, this);
}

void CutTask::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_jobProcess->readAllStandardError());
    m_logDetails.append(buffer);
    int progress = 0;
    // Parse FFmpeg output
    if (m_jobDuration == 0) {
        if (buffer.contains(QLatin1String("Duration:"))) {
            QString data = buffer.section(QStringLiteral("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
            if (!data.isEmpty()) {
                QStringList numbers = data.split(QLatin1Char(':'));
                if (numbers.size() < 3) {
                    return;
                }
                m_jobDuration = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toInt();
            }
        }
    } else if (buffer.contains(QLatin1String("time="))) {
        QString time = buffer.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
        if (!time.isEmpty()) {
            QStringList numbers = time.split(QLatin1Char(':'));
            if (numbers.size() < 3) {
                progress = time.toInt();
                if (progress == 0) {
                    return;
                }
            } else {
                progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + qRound(numbers.at(2).toDouble());
            }
        }
        m_progress = 100 * progress / m_jobDuration;
        QMetaObject::invokeMethod(m_object, "updateJobProgress");
    }
}
