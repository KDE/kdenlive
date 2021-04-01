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
#include "mainwindow.h"
#include "bin/model/subtitlemodel.hpp"
#include "kdenlive_debug.h"

#include "mlt++/MltProfile.h"
#include "mlt++/MltTractor.h"
#include "mlt++/MltConsumer.h"

#include <KLocalizedString>
#include <KMessageWidget>
#include <QDir>
#include <QFontDatabase>
#include <QProcess>
#include <memory>
#include <utility>

SpeechDialog::SpeechDialog(std::shared_ptr<TimelineItemModel> timeline, QPoint zone, bool, bool, QWidget *parent)
    : QDialog(parent)
    , m_timeline(timeline)
    
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Process"));
    speech_info->hide();
    m_voskConfig = new QAction(i18n("Configure"), this);
    connect(m_voskConfig, &QAction::triggered, []() {
        pCore->window()->slotPreferences(8);
    });
    m_modelsConnection = connect(pCore.get(), &Core::voskModelUpdate, [&](QStringList models) {
        language_box->clear();
        language_box->addItems(models);
        if (models.isEmpty()) {
            speech_info->addAction(m_voskConfig);
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
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, [this, zone]() {
        slotProcessSpeech(zone);
    });
    parseVoskDictionaries();
    frame_progress->setVisible(false);
    button_abort->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
    connect(button_abort, &QToolButton::clicked, [this]() {
        if (m_speechJob && m_speechJob->state() == QProcess::Running) {
            m_speechJob->kill();
        }
    });
}

SpeechDialog::~SpeechDialog()
{
    QObject::disconnect(m_modelsConnection);
}

void SpeechDialog::slotProcessSpeech(QPoint zone)
{
#ifdef Q_OS_WIN
    QString pyExec = QStandardPaths::findExecutable(QStringLiteral("python"));
#else
    QString pyExec = QStandardPaths::findExecutable(QStringLiteral("python3"));
#endif
    if (pyExec.isEmpty()) {
        speech_info->removeAction(m_voskConfig);
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("Cannot find python3, please install it on your system."));
        speech_info->animatedShow();
        return;
    }
    if (!KdenliveSettings::vosk_found() || !KdenliveSettings::vosk_srt_found()) {
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("Please configure speech to text."));
        speech_info->animatedShow();
        speech_info->addAction(m_voskConfig);
        return;
    }
    speech_info->removeAction(m_voskConfig);
    speech_info->setMessageType(KMessageWidget::Information);
    speech_info->setText(i18n("Starting audio export"));
    speech_info->show();
    qApp->processEvents();
    QString sceneList;
    QString speech;
    QString audio;
    QTemporaryFile tmpPlaylist(QDir::temp().absoluteFilePath(QStringLiteral("XXXXXX.mlt")));
    m_tmpSrt = std::make_unique<QTemporaryFile>(QDir::temp().absoluteFilePath(QStringLiteral("XXXXXX.srt")));
    m_tmpAudio = std::make_unique<QTemporaryFile>(QDir::temp().absoluteFilePath(QStringLiteral("XXXXXX.wav")));
    if (tmpPlaylist.open()) {
        sceneList = tmpPlaylist.fileName();
    }
    tmpPlaylist.close();    
    if (m_tmpSrt->open()) {
        speech = m_tmpSrt->fileName();
    }
    m_tmpSrt->close();
    if (m_tmpAudio->open()) {
        audio = m_tmpAudio->fileName();
    }
    m_tmpAudio->close();
    pCore->getMonitor(Kdenlive::ProjectMonitor)->sceneList(QDir::temp().absolutePath(), sceneList);
    Mlt::Producer producer(*m_timeline->tractor()->profile(), "xml", sceneList.toUtf8().constData());
    qDebug()<<"=== STARTING RENDER B";
    Mlt::Consumer xmlConsumer(*m_timeline->tractor()->profile(), "avformat", audio.toUtf8().constData());
    QString speechScript = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/speech.py"));
    if (speechScript.isEmpty()) {
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("The speech script was not found, check your install."));
        speech_info->animatedShow();
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        return;
    }
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
    speech_progress->setValue(0);
    frame_progress->setVisible(true);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    qApp->processEvents();
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.set("properties", "WAV");
    producer.set_in_and_out(zone.x(), zone.y());
    xmlConsumer.connect(producer);
    qDebug()<<"=== STARTING RENDER C, IN:"<<zone.x()<<" - "<<zone.y();
    m_duration = zone.y() - zone.x();
    qApp->processEvents();
    xmlConsumer.run();
    qApp->processEvents();
    qDebug()<<"=== STARTING RENDER D";
    QString language = language_box->currentText();
    qDebug()<<"=== RUNNING SPEECH ANALYSIS: "<<speechScript;
    speech_info->setMessageType(KMessageWidget::Information);
    speech_info->setText(i18n("Starting speech recognition"));
    qApp->processEvents();
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    }
    qDebug()<<"==== ANALYSIS SPEECH: "<<modelDirectory<<" - "<<language<<" - "<<audio<<" - "<<speech;
    m_speechJob = std::make_unique<QProcess>(this);
    connect(m_speechJob.get(), &QProcess::readyReadStandardOutput, this, &SpeechDialog::slotProcessProgress);
    connect(m_speechJob.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), [this, speech, zone](int, QProcess::ExitStatus status) {
       slotProcessSpeechStatus(status, speech, zone);
    });
    m_speechJob->start(pyExec, {speechScript, modelDirectory, language, audio, speech});
}

void SpeechDialog::slotProcessSpeechStatus(QProcess::ExitStatus status, const QString &srtFile, const QPoint zone)
{
    qDebug()<<"/// TERMINATING SPEECH JOB\n\n+++++++++++++++++++++++++++";
    if (status == QProcess::CrashExit) {
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("Speech recognition aborted."));
        speech_info->animatedShow();
    } else {
        if (QFile::exists(srtFile)) {
            m_timeline->getSubtitleModel()->importSubtitle(srtFile, zone.x(), true);
            speech_info->setMessageType(KMessageWidget::Positive);
            speech_info->setText(i18n("Subtitles imported"));
        } else {
            speech_info->setMessageType(KMessageWidget::Warning);
            speech_info->setText(i18n("Speech recognition failed"));
        }
    }
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    frame_progress->setVisible(false);
}

void SpeechDialog::slotProcessProgress()
{
     QString saveData = QString::fromUtf8(m_speechJob->readAll());
     qDebug()<<"==== GOT SPEECH DATA: "<<saveData;
     if (saveData.startsWith(QStringLiteral("progress:"))) {
         double prog = saveData.section(QLatin1Char(':'), 1).toInt() * 3.12;
        qDebug()<<"=== GOT DATA:\n"<<saveData;
        speech_progress->setValue(static_cast<int>(100 * prog / m_duration));
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
