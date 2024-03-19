/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "cuttask.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "mainwindow.h"
#include "profiles/profilemodel.hpp"
#include "ui_cutjobdialog_ui.h"
#include "utils/qstringutils.h"
#include "xml/xml.hpp"

#include <KIO/RenameDialog>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox>
#include <QComboBox>
#include <QObject>
#include <QProcess>
#include <QThread>

CutTask::CutTask(const ObjectId &owner, const QString &destination, const QStringList &encodingParams, int in, int out, bool addToProject, QObject *object)
    : AbstractTask(owner, AbstractTask::CUTJOB, object)
    , m_inPoint(GenTime(in, pCore->getCurrentFps()))
    , m_outPoint(GenTime(out, pCore->getCurrentFps()))
    , m_destination(destination)
    , m_encodingParams(encodingParams)
    , m_jobDuration(0)
    , m_addToProject(addToProject)
{
    m_description = i18n("Extracting zone");
}

void CutTask::start(const ObjectId &owner, int in, int out, QObject *object, bool force)
{
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(owner.itemId));
    ClipType::ProducerType type = binClip->clipType();
    if (type != ClipType::AV && type != ClipType::Audio && type != ClipType::Video) {
        // m_errorMessage.prepend(i18n("Cannot extract zone for this clip type."));
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

    QFileInfo finfo(source);
    QDir dir = finfo.absoluteDir();
    QString inString = QString::number(int(GenTime(in, pCore->getCurrentFps()).seconds()));
    QString outString = QString::number(int(GenTime(out, pCore->getCurrentFps()).seconds()));
    QString fileName = QStringUtils::appendToFilename(finfo.fileName(), QString("-%1-%2").arg(inString, outString));
    QString path = dir.absoluteFilePath(fileName);

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
        ui.vcodec->addItem(i18n("Disable stream"));
    }
    if (audioCodec.isEmpty()) {
        ui.audio_codec->setText(i18n("none"));
        ui.acodec->setEnabled(false);
    } else {
        ui.audio_codec->setText(audioCodec);
        ui.acodec->addItem(i18n("Copy stream"), QStringLiteral("copy"));
        ui.acodec->addItem(i18n("PCM encoding"), QStringLiteral("pcm_s24le"));
        ui.acodec->addItem(i18n("AAC encoding"), QStringLiteral("aac"));
        ui.acodec->addItem(i18n("Disable stream"));
    }
    ui.audio_codec->setText(audioCodec);
    ui.add_clip->setChecked(KdenliveSettings::add_new_clip());
    ui.file_url->setMode(KFile::File);
    ui.extra_params->setMaximumHeight(QFontMetrics(QApplication::font()).lineSpacing() * 5);
    ui.file_url->setUrl(QUrl::fromLocalFile(path));

    QString transcoderExt = QLatin1Char('.') + finfo.suffix();

    std::function<void()> callBack = [&ui, transcoderExt]() {
        if (ui.acodec->currentData().toString().isEmpty()) {
            // Video only
            ui.extra_params->setPlainText(QString("-an -c:v %1").arg(ui.vcodec->currentData().toString()));
        } else if (ui.vcodec->currentData().toString().isEmpty()) {
            // Audio only
            ui.extra_params->setPlainText(QString("-vn -c:a %1").arg(ui.acodec->currentData().toString()));
        } else {
            ui.extra_params->setPlainText(QString("-c:a %1 -c:v %2").arg(ui.acodec->currentData().toString(), ui.vcodec->currentData().toString()));
        }
        QString path = ui.file_url->url().toLocalFile();
        QString fileName = path.section(QLatin1Char('.'), 0, -2);
        if (ui.acodec->currentData().toString() == QLatin1String("copy") && ui.vcodec->currentData().toString() == QLatin1String("copy")) {
            fileName.append(transcoderExt);
        } else {
            fileName.append(QStringLiteral(".mov"));
        }
        ui.file_url->setUrl(QUrl::fromLocalFile(fileName));
    };

    QObject::connect(ui.acodec, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), d.data(), [callBack]() { callBack(); });

    QObject::connect(ui.vcodec, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), d.data(), [callBack]() { callBack(); });
    QFontMetrics fm = ui.file_url->lineEdit()->fontMetrics();
    ui.file_url->setMinimumWidth(int(fm.boundingRect(ui.file_url->text().left(50)).width() * 1.4));
    callBack();
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
    QStringList encodingParams = ui.extra_params->toPlainText().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    KdenliveSettings::setAdd_new_clip(ui.add_clip->isChecked());
    delete d;

    if (QFile::exists(path)) {
        KIO::RenameDialog renameDialog(qApp->activeWindow(), i18n("File already exists"), QUrl::fromLocalFile(path), QUrl::fromLocalFile(path),
                                       KIO::RenameDialog_Option::RenameDialog_Overwrite);
        if (renameDialog.exec() != QDialog::Rejected) {
            QUrl final = renameDialog.newDestUrl();
            if (final.isValid()) {
                path = final.toLocalFile();
            }
        } else {
            return;
        }
    }
    CutTask *task = new CutTask(owner, path, encodingParams, in, out, KdenliveSettings::add_new_clip(), object);
    // Otherwise, start a filter thread.
    task->m_isForce = force;
    pCore->taskManager.startTask(owner.itemId, task);
}

void CutTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        return;
    }
    QMutexLocker lock(&m_runMutex);
    m_running = true;
    qDebug() << " + + + + + + + + STARTING STAB TASK";

    QString url;
    auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
    if (binClip) {
        // Filter applied on a timeline or bin clip
        url = binClip->url();
        QString folder = QStringLiteral("-1");
        auto containingFolder = std::static_pointer_cast<ProjectFolder>(binClip->parent());
        if (containingFolder) {
            folder = containingFolder->clipId();
        }
        if (url.isEmpty()) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("No producer for this clip.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)));
            m_errorMessage.append(i18n("No producer for this clip."));
            return;
        }
        if (QFileInfo(m_destination).absoluteFilePath() == QFileInfo(url).absoluteFilePath()) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("You cannot overwrite original clip.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)));
            m_errorMessage.append(i18n("You cannot overwrite original clip."));
            return;
        }
        QStringList params = {QStringLiteral("-y"),
                              QStringLiteral("-stats"),
                              QStringLiteral("-v"),
                              QStringLiteral("error"),
                              QStringLiteral("-noaccurate_seek"),
                              QStringLiteral("-ss"),
                              QString::number(m_inPoint.seconds()),
                              QStringLiteral("-i"),
                              url,
                              QStringLiteral("-t"),
                              QString::number((m_outPoint - m_inPoint).seconds()),
                              QStringLiteral("-avoid_negative_ts"),
                              QStringLiteral("make_zero"),
                              QStringLiteral("-sn"),
                              QStringLiteral("-dn"),
                              QStringLiteral("-map"),
                              QStringLiteral("0")};
        params << m_encodingParams << m_destination;
        m_jobProcess = std::make_unique<QProcess>(new QProcess);
        connect(m_jobProcess.get(), &QProcess::readyReadStandardError, this, &CutTask::processLogInfo);
        connect(this, &CutTask::jobCanceled, m_jobProcess.get(), &QProcess::kill, Qt::DirectConnection);
        qDebug() << "=== STARTING CUT JOB: " << params;
        m_jobProcess->start(KdenliveSettings::ffmpegpath(), params, QIODevice::ReadOnly);
        m_jobProcess->waitForFinished(-1);
        bool result = m_jobProcess->exitStatus() == QProcess::NormalExit;
        // remove temporary playlist if it exists
        if (result && !m_isCanceled) {
            if (QFileInfo(m_destination).size() == 0) {
                QFile::remove(m_destination);
                // File was not created
                QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Failed to create file.")),
                                          Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
            } else {
                // all ok, add clip
                if (m_addToProject) {
                    QMetaObject::invokeMethod(pCore->window(), "addProjectClip", Qt::QueuedConnection, Q_ARG(QString, m_destination), Q_ARG(QString, folder));
                }
            }
        } else {
            // transcode task crashed
            QFile::remove(m_destination);
            QMetaObject::invokeMethod(pCore.get(), "displayBinLogMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Cut job failed.")),
                                      Q_ARG(int, int(KMessageWidget::Warning)), Q_ARG(QString, m_logDetails));
        }
    }
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
