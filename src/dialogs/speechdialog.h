/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_speechdialog_ui.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include "definitions.h"
#include "pythoninterfaces/speechtotext.h"


#include <QProcess>
#include <QTemporaryFile>

class QAction;

/**
 * @class SpeechDialog
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */
class SpeechDialog : public QDialog, public Ui::SpeechDialog_UI
{
    Q_OBJECT

public:
    explicit SpeechDialog(std::shared_ptr<TimelineItemModel> timeline, QPoint zone, int tid, bool activeTrackOnly = false, bool selectionOnly = false,
                          QWidget *parent = nullptr);
    ~SpeechDialog() override;

private:
    std::unique_ptr<QProcess> m_speechJob;
    const std::shared_ptr<TimelineItemModel> m_timeline;
    QPoint m_zone;
    int m_tid;
    int m_duration;
    std::unique_ptr<QTemporaryFile> m_tmpAudio;
    std::unique_ptr<QTemporaryFile> m_tmpSrt;
    QAction *m_voskConfig;
    QAction *m_logAction;
    QString m_errorLog;
    SpeechToText *m_stt;
    void fillSeamlessLanguages();

private Q_SLOTS:
    void slotProcessSpeech();
    void slotProcessSpeechStatus(QProcess::ExitStatus status, const QString &srtFile);
    void slotProcessProgress();
    void slotProcessWhisperProgress();
    void updateVoskModels(const QStringList models);
    void checkDeps();
};
