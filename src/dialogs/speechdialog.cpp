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

#include <QFontDatabase>
#include <QDir>
#include <QProcess>
#include <KLocalizedString>
#include <KMessageWidget>

SpeechDialog::SpeechDialog(const std::shared_ptr<TimelineItemModel> &timeline, QPoint zone, bool activeTrackOnly, bool selectionOnly, QWidget *parent)
    : QDialog(parent)
    
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Process"));
    speech_info->hide();
    connect(pCore.get(), &Core::updateVoskAvailability, this, &SpeechDialog::updateAvailability);
    connect(pCore.get(), &Core::voskModelUpdate, [&](QStringList models) {
        language_box->clear();
        language_box->addItems(models);
        updateAvailability();
        if (models.isEmpty()) {
            speech_info->setMessageType(KMessageWidget::Information);
            speech_info->setText(i18n("Please install speech recognition models"));
            speech_info->animatedShow();
        } else {
            if (!KdenliveSettings::vosk_srt_model().isEmpty() && models.contains(KdenliveSettings::vosk_srt_model())) {
                int ix = language_box->findText(KdenliveSettings::vosk_srt_model());
                if (ix > -1) {
                    language_box->setCurrentIndex(ix);
                }
            }
        }
    });
    connect(language_box, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this]() {
        KdenliveSettings::setVosk_srt_model(language_box->currentText());
    });
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, [this, timeline, zone]() {
        slotProcessSpeech(timeline, zone);
    });
    parseVoskDictionaries();
}

void SpeechDialog::updateAvailability()
{
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(KdenliveSettings::vosk_found() && KdenliveSettings::vosk_srt_found() && language_box->count() > 0);
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
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    }
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

void SpeechDialog::parseVoskDictionaries()
{
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    QDir dir;
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        dir = QDir(modelDirectory);
        if (!dir.cd(QStringLiteral("speechmodels"))) {
            qDebug()<<"=== /// CANNOT ACCESS SPEECH DICTIONARIES FOLDER";
            pCore->voskModelUpdate({});
            return;
        }
    } else {
        dir = QDir(modelDirectory);
    }
    QStringList dicts = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList final;
    for (auto &d : dicts) {
        QDir sub(dir.absoluteFilePath(d));
        if (sub.exists(QStringLiteral("mfcc.conf")) || (sub.exists(QStringLiteral("conf/mfcc.conf")))) {
            final << d;
        }
    }
    pCore->voskModelUpdate(final);
}
