/***************************************************************************
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

#include "speechdialog.h"

#include "core.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "bin/model/subtitlemodel.hpp"
#include "kdenlive_debug.h"

#include "mlt++/MltProfile.h"
#include "mlt++/MltTractor.h"
#include "mlt++/MltConsumer.h"

#include <kio_version.h>
#include <QFontDatabase>
#include <QDir>
#include <QProcess>
#include <KLocalizedString>
#include <KUrlRequesterDialog>
#include <KArchive>
#include <KZip>
#include <KTar>
#include <KIO/FileCopyJob>
#if KIO_VERSION > QT_VERSION_CHECK(5, 70, 0)
#include <KIO/OpenUrlJob>
#endif
#include <KIO/JobUiDelegate>
#include <KArchiveDirectory>
#include <KMessageWidget>

SpeechDialog::SpeechDialog(const std::shared_ptr<TimelineItemModel> &timeline, QPoint zone, bool activeTrackOnly, bool selectionOnly, QWidget *parent)
    : QDialog(parent)
    
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Process"));
    dict_info->hide();
    speech_info->hide();
    slotParseDictionaries();
    button_add->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    button_delete->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    connect(button_add, &QToolButton::clicked, this, &SpeechDialog::getDictionary);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, [this, timeline, zone]() {
        slotProcessSpeech(timeline, zone);
    });
    connect(dict_info, &KMessageWidget::linkActivated, [&](const QString &contents) {
        qDebug()<<"=== LINK CLICKED: "<<contents;
#if KIO_VERSION > QT_VERSION_CHECK(5, 70, 0)
        auto *job = new KIO::OpenUrlJob(QUrl(contents));
        job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        // methods like setRunExecutables, setSuggestedFilename, setEnableExternalBrowser, setFollowRedirections
        // exist in both classes
        job->start();
#endif
    });
    //TODO: check for the python scripts vosk and srt

    connect(this, &SpeechDialog::parseDictionaries, this, &SpeechDialog::slotParseDictionaries);
}
    
void SpeechDialog::slotProcessSpeech(const std::shared_ptr<TimelineItemModel> &timeline, QPoint zone)
{
    QString pyExec = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (pyExec.isEmpty()) {
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("Cannot find python3, please install it on your system."));
        speech_info->animatedShow();
        return;
    }
    speech_info->setMessageType(KMessageWidget::Information);
    speech_info->setText(i18n("Starting audio export"));
    speech_info->show();
    qApp->processEvents();
    QString sceneList;
    QString speech;
    QString audio;
    QTemporaryFile tmpPlaylist(QDir::temp().absoluteFilePath(QStringLiteral("XXXXXX.mlt")));
    QTemporaryFile tmpSpeech(QDir::temp().absoluteFilePath(QStringLiteral("XXXXXX.srt")));
    QTemporaryFile tmpAudio(QDir::temp().absoluteFilePath(QStringLiteral("XXXXXX.wav")));
    if (tmpPlaylist.open()) {
        sceneList = tmpPlaylist.fileName();
    }
    tmpPlaylist.close();    
    if (tmpSpeech.open()) {
        speech = tmpSpeech.fileName();
    }
    tmpSpeech.close();
    if (tmpAudio.open()) {
        audio = tmpAudio.fileName();
    }
    tmpAudio.close();
    pCore->getMonitor(Kdenlive::ProjectMonitor)->sceneList(QDir::temp().absolutePath(), sceneList);
    Mlt::Producer producer(*timeline->tractor()->profile(), "xml", sceneList.toUtf8().constData());
    qDebug()<<"=== STARTING RENDER B";
    Mlt::Consumer xmlConsumer(*timeline->tractor()->profile(), "avformat", audio.toUtf8().constData());
    qApp->processEvents();
    if (!xmlConsumer.is_valid() || !producer.is_valid()) {
        qDebug()<<"=== STARTING CONSUMER ERROR";
        if (!producer.is_valid()) {
            qDebug()<<"=== PRODUCER INVALID";
        }
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("Audio export failed"));
        qApp->processEvents();
        return;
    }
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.set("properties", "WAV");
    producer.set_in_and_out(zone.x(), zone.y());
    xmlConsumer.connect(producer);
    qDebug()<<"=== STARTING RENDER C, IN:"<<zone.x()<<" - "<<zone.y();
    qApp->processEvents();
    xmlConsumer.run();
    qApp->processEvents();
    qDebug()<<"=== STARTING RENDER D";
    QString language = language_box->currentText();
    QString speechScript = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/speech.py"));
    if (speechScript.isEmpty()) {
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("The speech script was not found, check your install."));
        speech_info->animatedShow();
        return;
    }

    qDebug()<<"=== RUNNING SPEECH ANALYSIS: "<<speechScript;
    QProcess speechJob;
    speech_info->setMessageType(KMessageWidget::Information);
    speech_info->setText(i18n("Starting speech recognition"));
    qApp->processEvents();
    QString modelDirectory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    qDebug()<<"==== ANALYSIS SPEECH: "<<modelDirectory<<" - "<<language<<" - "<<audio<<" - "<<speech;
    speechJob.start(pyExec, {speechScript, modelDirectory, language, audio, speech});
    speechJob.waitForFinished();
    if (QFile::exists(speech)) {
        timeline->getSubtitleModel()->importSubtitle(speech, zone.x(), true);
        speech_info->setMessageType(KMessageWidget::Positive);
        speech_info->setText(i18n("Subtitles imported"));
    } else {
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("Speech recognition failed"));
    }
}


void SpeechDialog::getDictionary()
{
    QUrl url = KUrlRequesterDialog::getUrl(QUrl(), this, i18n("Enter url for the new dictionary"));
    if (url.isEmpty()) {
        return;
    }
    QString tmpFile;
    if (!url.isLocalFile()) {
        KIO::FileCopyJob *copyjob = KIO::file_copy(url, QUrl::fromLocalFile(QDir::temp().absoluteFilePath(url.fileName())));
        dict_info->setMessageType(KMessageWidget::Information);
        dict_info->setText(i18n("Downloading model..."));
        dict_info->animatedShow();
        connect(copyjob, &KIO::FileCopyJob::result, this, &SpeechDialog::processArchive);
        /*if (copyjob->exec()) {
            qDebug()<<"=== GOT REST: "<<copyjob->destUrl();
            //
        } else {
            qDebug()<<"=== CANNOT DOWNLOAD";
        }*/
    } else {
        //KMessageBox::error(this, KIO::NetAccess::lastErrorString());
        //KArchive ar(tmpFile);
    }
    
}

void SpeechDialog::processArchive(KJob* job)
{
    qDebug()<<"=== DOWNLOAD FINISHED!!";
    if (job->error() == 0 || job->error() == 112) {
        qDebug()<<"=== NO ERROR ON DWNLD!!";
        KIO::FileCopyJob *jb = static_cast<KIO::FileCopyJob*>(job);
        if (jb) {
            qDebug()<<"=== JOB FOUND!!";
            QMimeDatabase db;
            QString archiveFile = jb->destUrl().toLocalFile();
            QMimeType type = db.mimeTypeForFile(archiveFile);
            std::unique_ptr<KArchive> archive;
            if (type.inherits(QStringLiteral("application/zip"))) {
                archive.reset(new KZip(archiveFile));
            } else {
                archive.reset(new KTar(archiveFile));
            }
            QString modelDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir dir(modelDirectory);
            dir.mkdir(QStringLiteral("speechmodels"));
            if (!dir.cd(QStringLiteral("speechmodels"))) {
                qDebug()<<"=== /// CANNOT ACCESS SPEECH DICTIONARIES FOLDER";
                dict_info->setMessageType(KMessageWidget::Warning);
                dict_info->setText(i18n("Cannot access dictionary folder"));
                return;
            }
            if (archive->open(QIODevice::ReadOnly)) {
                dict_info->setText(i18n("Extracting archive..."));
                const KArchiveDirectory *archiveDir = archive->directory();
                if (!archiveDir->copyTo(dir.absolutePath())) {
                    qDebug()<<"=== Error extracting archive!!";
                } else {
                    QFile::remove(archiveFile);
                    emit parseDictionaries();
                    dict_info->setMessageType(KMessageWidget::Positive);
                    dict_info->setText(i18n("New dictionary installed"));
                }
            } else {
                qDebug()<<"=== CANNOT OPEN ARCHIVE!!";
            }
        } else {
            qDebug()<<"=== JOB NOT FOUND!!";
            dict_info->setMessageType(KMessageWidget::Warning);
            dict_info->setText(i18n("Download error"));
        }
    } else {
        qDebug()<<"=== GOT JOB ERROR: "<<job->error();
        dict_info->setMessageType(KMessageWidget::Warning);
        dict_info->setText(i18n("Download error %1", job->errorString()));
    }
}

void SpeechDialog::slotParseDictionaries()
{
    listWidget->clear();
    language_box->clear();
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    QString modelDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(modelDirectory);
    if (!dir.cd(QStringLiteral("speechmodels"))) {
        qDebug()<<"=== /// CANNOT ACCESS SPEECH DICTIONARIES FOLDER";
        tabWidget->setCurrentIndex(1);
        dict_info->setMessageType(KMessageWidget::Information);
        dict_info->setText(i18n("Download dictionaries from: <a href=\"https://alphacephei.com/vosk/models\">https://alphacephei.com/vosk/models</a>"));
        dict_info->animatedShow();
        return;
    }
    QStringList dicts = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    listWidget->addItems(dicts);
    language_box->addItems(dicts);
    if (!dicts.isEmpty()) {
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        dict_info->animatedHide();
    } else {
        tabWidget->setCurrentIndex(1);
        dict_info->setMessageType(KMessageWidget::Information);
        dict_info->setText(i18n("Download dictionaries from: <a href=\"https://alphacephei.com/vosk/models\">https://alphacephei.com/vosk/models</a>"));
        dict_info->animatedShow();
    }
}
