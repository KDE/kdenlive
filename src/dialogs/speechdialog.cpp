/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "speechdialog.h"

#include "bin/model/subtitlemodel.hpp"
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitor.h"

#include "mlt++/MltConsumer.h"
#include "mlt++/MltProfile.h"
#include "mlt++/MltTractor.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>
#include <QButtonGroup>
#include <QDir>
#include <QFontDatabase>
#include <QProcess>
#include <kwidgetsaddons_version.h>

#include <memory>
#include <utility>

SpeechDialog::SpeechDialog(std::shared_ptr<TimelineItemModel> timeline, QPoint zone, int tid, bool, bool, QWidget *parent)
    : QDialog(parent)
    , m_timeline(timeline)
    , m_zone(zone)
    , m_tid(-1)

{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    speech_info->hide();
    setWindowTitle(i18n("Automatic Subtitling"));
    m_voskConfig = new QAction(i18n("Configure"), this);
    connect(m_voskConfig, &QAction::triggered, [this]() {
        pCore->window()->slotShowPreferencePage(Kdenlive::PageSpeech);
        close();
    });
    m_logAction = new QAction(i18n("Show log"), this);
    connect(m_logAction, &QAction::triggered, [&]() { KMessageBox::detailedError(QApplication::activeWindow(), i18n("Speech Recognition log"), m_errorLog); });

    if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
        // Whisper model
        m_stt = new SpeechToText(SpeechToText::EngineType::EngineWhisper);
        QList<std::pair<QString, QString>> whisperModels = m_stt->whisperModels();
        for (auto &w : whisperModels) {
            speech_model->addItem(w.first, w.second);
        }
        int ix = speech_model->findData(KdenliveSettings::whisperModel());
        if (ix > -1) {
            speech_model->setCurrentIndex(ix);
        }
        if (speech_language->count() == 0) {
            // Fill whisper languages
            QMap<QString, QString> languages = m_stt->whisperLanguages();
            QMapIterator<QString, QString> j(languages);
            while (j.hasNext()) {
                j.next();
                speech_language->addItem(j.key(), j.value());
            }
            int ix = speech_language->findData(KdenliveSettings::whisperLanguage());
            if (ix > -1) {
                speech_language->setCurrentIndex(ix);
            }
        }
        speech_language->setEnabled(!KdenliveSettings::whisperModel().endsWith(QLatin1String(".en")));
        translate_box->setChecked(KdenliveSettings::whisperTranslate());

    } else {
        // Vosk model
        whisper_settings->setVisible(false);
        m_stt = new SpeechToText(SpeechToText::EngineType::EngineVosk);
        connect(pCore.get(), &Core::voskModelUpdate, this, &SpeechDialog::updateVoskModels);
        m_stt->parseVoskDictionaries();
    }
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Process"));

    QButtonGroup *buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(timeline_full);
    buttonGroup->addButton(timeline_zone);
    buttonGroup->addButton(timeline_track);
    buttonGroup->addButton(timeline_clips);
    connect(buttonGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), [=, selectedTrack = tid, sourceZone = zone](QAbstractButton *button) {
        speech_info->animatedHide();
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        if (button == timeline_full) {
            m_tid = -1;
            m_zone = QPoint(0, pCore->projectDuration() - 1);
        } else if (button == timeline_clips) {
            std::unordered_set<int> selection = timeline->getCurrentSelection();
            int cid = -1;
            m_tid = -1;
            int firstPos = -1;
            for (const auto &s : selection) {
                // Find first clip
                if (!timeline->isClip(s)) {
                    continue;
                }
                int pos = timeline->getClipPosition(s);
                if (firstPos == -1 || pos < firstPos) {
                    cid = s;
                    firstPos = pos;
                    m_tid = timeline->getClipTrackId(cid);
                    if (!timeline->isAudioTrack(m_tid)) {
                        m_tid = timeline->getMirrorAudioTrackId(m_tid);
                    }
                }
            }
            if (m_tid == -1) {
                speech_info->setMessageType(KMessageWidget::Information);
                speech_info->setText(i18n("No audio track available for selected clip"));
                speech_info->animatedShow();
                buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
                return;
            }
            if (timeline->isClip(cid)) {
                m_zone.setX(timeline->getClipPosition(cid));
                m_zone.setY(m_zone.x() + timeline->getClipPlaytime(cid));
            } else {
                speech_info->setMessageType(KMessageWidget::Information);
                speech_info->setText(i18n("Select a clip in timeline to perform analysis"));
                speech_info->animatedShow();
                buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
            }
        } else {
            if (button == timeline_track) {
                m_tid = selectedTrack;
                if (timeline->isSubtitleTrack(m_tid)) {
                    m_tid = -1;
                } else if (!timeline->isAudioTrack(m_tid)) {
                    m_tid = timeline->getMirrorAudioTrackId(m_tid);
                }
                if (m_tid == -1) {
                    speech_info->setMessageType(KMessageWidget::Information);
                    speech_info->setText(i18n("No audio track found"));
                    speech_info->animatedShow();
                    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
                }
            } else {
                m_tid = -1;
            }
            m_zone = sourceZone;
        }
    });
    connect(speech_model, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this, [this]() {
        if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
            const QString modelName = speech_model->currentData().toString();
            KdenliveSettings::setWhisperModel(modelName);
            speech_language->setEnabled(!modelName.endsWith(QLatin1String(".en")));
        } else {
            KdenliveSettings::setVosk_srt_model(speech_model->currentText());
        }
    });
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() { slotProcessSpeech(); });
    frame_progress->setVisible(false);
    connect(button_abort, &QToolButton::clicked, this, [this]() {
        if (m_speechJob && m_speechJob->state() == QProcess::Running) {
            m_speechJob->kill();
        }
    });
    m_stt->checkDependencies();
}

SpeechDialog::~SpeechDialog() {}

void SpeechDialog::updateVoskModels(const QStringList models)
{
    speech_model->clear();
    speech_model->addItems(models);
    if (models.isEmpty()) {
        speech_info->addAction(m_voskConfig);
        speech_info->setMessageType(KMessageWidget::Information);
        speech_info->setText(i18n("Please install speech recognition models"));
        speech_info->show();
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    } else {
        if (!KdenliveSettings::vosk_srt_model().isEmpty() && models.contains(KdenliveSettings::vosk_srt_model())) {
            int ix = speech_model->findText(KdenliveSettings::vosk_srt_model());
            if (ix > -1) {
                speech_model->setCurrentIndex(ix);
            }
        }
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
    }
}

void SpeechDialog::slotProcessSpeech()
{
    speech_info->setMessageType(KMessageWidget::Information);
    speech_info->setText(i18nc("@label:textbox", "Checking setup…"));
    speech_info->show();
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    if (!m_stt->checkSetup() || !m_stt->missingDependencies().isEmpty()) {
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
    m_timeline->sceneList(QDir::temp().absolutePath(), sceneList);
    // TODO: do the rendering in another thread to not block the UI

    Mlt::Producer producer(m_timeline->tractor()->get_profile(), "xml", sceneList.toUtf8().constData());
    int tracksCount = m_timeline->tractor()->count();
    std::shared_ptr<Mlt::Service> s(new Mlt::Service(producer));
    std::shared_ptr<Mlt::Multitrack> multi = nullptr;
    bool multitrackFound = false;
    for (int i = 0; i < 10; i++) {
        s.reset(s->producer());
        if (s == nullptr || !s->is_valid()) {
            break;
        }
        if (s->type() == mlt_service_multitrack_type) {
            multi.reset(new Mlt::Multitrack(*s.get()));
            if (multi->count() == tracksCount) {
                // Match
                multitrackFound = true;
                break;
            }
        }
    }
    if (multitrackFound) {
        int trackPos = -1;
        if (m_tid > -1) {
            trackPos = m_timeline->getTrackMltIndex(m_tid);
        }
        int tid = 0;
        for (int i = 0; i < multi->count(); i++) {
            std::shared_ptr<Mlt::Producer> tk(multi->track(i));
            if (tk->get_int("hide") == 1) {
                // Video track, hide it
                tk->set("hide", 3);
            } else if (tid == 0 || (trackPos > -1 && trackPos != tid)) {
                // We only want a specific audio track
                tk->set("hide", 3);
            }
            tid++;
        }
    }
    Mlt::Consumer xmlConsumer(m_timeline->tractor()->get_profile(), "avformat", audio.toUtf8().constData());
    if (!xmlConsumer.is_valid() || !producer.is_valid()) {
        qDebug() << "=== STARTING CONSUMER ERROR";
        if (!producer.is_valid()) {
            qDebug() << "=== PRODUCER INVALID";
        }
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("Audio export failed"));
        qApp->processEvents();
        return;
    }
    speech_progress->setValue(0);
    m_errorLog.clear();
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 100, 0)
    speech_info->clearActions();
#else
    speech_info->removeAction(m_logAction);
#endif
    frame_progress->setVisible(true);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    qApp->processEvents();
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.set("properties", "WAV");
    producer.set_in_and_out(m_zone.x(), m_zone.y());
    xmlConsumer.connect(producer);

    qDebug() << "=== STARTING RENDER C, IN:" << m_zone.x() << " - " << m_zone.y();
    m_duration = m_zone.y() - m_zone.x();
    qApp->processEvents();
    xmlConsumer.run();
    qApp->processEvents();
    qDebug() << "=== STARTING RENDER D";
    speech_info->setMessageType(KMessageWidget::Information);
    speech_info->setText(i18n("Starting speech recognition"));
    qApp->processEvents();
    QString modelDirectory = m_stt->voskModelPath();
    m_speechJob = std::make_unique<QProcess>(this);
    connect(m_speechJob.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this,
            [this, speech](int, QProcess::ExitStatus status) { slotProcessSpeechStatus(status, speech); });
    if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
        // Whisper
        QString modelName = speech_model->currentData().toString();
        m_speechJob->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_speechJob.get(), &QProcess::readyReadStandardOutput, this, &SpeechDialog::slotProcessWhisperProgress);
        QString language = speech_language->isEnabled() ? speech_language->currentData().toString().simplified() : QString();
        if (!language.isEmpty()) {
            language.prepend(QStringLiteral("language="));
        }
        qDebug() << "==== ANALYSIS SPEECH: " << m_stt->subtitleScript() << " " << audio << " " << modelName << " " << speech << " "
                 << KdenliveSettings::whisperDevice() << " " << (translate_box->isChecked() ? QStringLiteral("translate") : QStringLiteral("transcribe")) << " "
                 << language;
        if (KdenliveSettings::whisperDisableFP16()) {
            language.append(QStringLiteral(" fp16=False"));
        }
        m_speechJob->start(m_stt->pythonExec(), {m_stt->subtitleScript(), audio, modelName, speech, KdenliveSettings::whisperDevice(),
                                                 translate_box->isChecked() ? QStringLiteral("translate") : QStringLiteral("transcribe"), language});
    } else {
        // Vosk
        QString modelName = speech_model->currentText();
        connect(m_speechJob.get(), &QProcess::readyReadStandardOutput, this, &SpeechDialog::slotProcessProgress);
        m_speechJob->start(m_stt->pythonExec(), {m_stt->subtitleScript(), modelDirectory, modelName, audio, speech});
    }
}

void SpeechDialog::slotProcessSpeechStatus(QProcess::ExitStatus status, const QString &srtFile)
{
    if (!m_errorLog.isEmpty()) {
        speech_info->addAction(m_logAction);
    }
    if (status == QProcess::CrashExit) {
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("Speech recognition aborted."));
        speech_info->animatedShow();
    } else {
        if (QFile::exists(srtFile)) {
            m_timeline->getSubtitleModel()->importSubtitle(srtFile, m_zone.x(), true);
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
    if (saveData.startsWith(QStringLiteral("progress:"))) {
        double prog = saveData.section(QLatin1Char(':'), 1).toInt() * 3.12;
        speech_progress->setValue(static_cast<int>(100 * prog / m_duration));
    }
}

void SpeechDialog::slotProcessWhisperProgress()
{
    QString saveData = QString::fromUtf8(m_speechJob->readAll());
    if (saveData.contains(QStringLiteral("%|"))) {
        int prog = saveData.section(QLatin1Char('%'), 0, 0).toInt();
        qDebug() << "=== GOT DATA:\n" << saveData << " = " << prog;
        speech_progress->setValue(prog);
    } else {
        m_errorLog.append(saveData);
    }
}
