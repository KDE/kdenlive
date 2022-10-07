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
#include <KMessageWidget>
#include <QButtonGroup>
#include <QDir>
#include <QFontDatabase>
#include <QProcess>

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
    m_stt = new SpeechToText();
    buttonBox->button(QDialogButtonBox::Apply)->setText(i18n("Process"));
    speech_info->hide();
    m_voskConfig = new QAction(i18n("Configure"), this);
    connect(m_voskConfig, &QAction::triggered, []() { pCore->window()->slotPreferences(8); });
    m_modelsConnection = connect(pCore.get(), &Core::voskModelUpdate, this, [&](const QStringList &models) {
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
    QButtonGroup *buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(timeline_zone);
    buttonGroup->addButton(timeline_track);
    buttonGroup->addButton(timeline_clips);
    connect(buttonGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), [=, selectedTrack = tid, sourceZone = zone](QAbstractButton *button) {
        speech_info->animatedHide();
        buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
        if (button == timeline_clips) {
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
                if (!timeline->isAudioTrack(m_tid)) {
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
    connect(language_box, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
            [this]() { KdenliveSettings::setVosk_srt_model(language_box->currentText()); });
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() { slotProcessSpeech(); });
    m_stt->parseVoskDictionaries();
    frame_progress->setVisible(false);
    connect(button_abort, &QToolButton::clicked, this, [this]() {
        if (m_speechJob && m_speechJob->state() == QProcess::Running) {
            m_speechJob->kill();
        }
    });
}

SpeechDialog::~SpeechDialog()
{
    QObject::disconnect(m_modelsConnection);
}

void SpeechDialog::slotProcessSpeech()
{
    m_stt->checkDependencies();
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

    Mlt::Producer producer(*m_timeline->tractor()->profile(), "xml", sceneList.toUtf8().constData());
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
    Mlt::Consumer xmlConsumer(*m_timeline->tractor()->profile(), "avformat", audio.toUtf8().constData());
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
    QString language = language_box->currentText();
    speech_info->setMessageType(KMessageWidget::Information);
    speech_info->setText(i18n("Starting speech recognition"));
    qApp->processEvents();
    QString modelDirectory = m_stt->voskModelPath();
    qDebug() << "==== ANALYSIS SPEECH: " << modelDirectory << " - " << language << " - " << audio << " - " << speech;
    m_speechJob = std::make_unique<QProcess>(this);
    connect(m_speechJob.get(), &QProcess::readyReadStandardOutput, this, &SpeechDialog::slotProcessProgress);
    connect(m_speechJob.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this,
            [this, speech](int, QProcess::ExitStatus status) { slotProcessSpeechStatus(status, speech); });
    m_speechJob->start(m_stt->pythonExec(), {m_stt->subtitleScript(), modelDirectory, language, audio, speech});
}

void SpeechDialog::slotProcessSpeechStatus(QProcess::ExitStatus status, const QString &srtFile)
{
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
    qDebug() << "==== GOT SPEECH DATA: " << saveData;
    if (saveData.startsWith(QStringLiteral("progress:"))) {
        double prog = saveData.section(QLatin1Char(':'), 1).toInt() * 3.12;
        qDebug() << "=== GOT DATA:\n" << saveData;
        speech_progress->setValue(static_cast<int>(100 * prog / m_duration));
    }
}
