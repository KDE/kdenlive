/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "pythoninterfaces/speechtotext.h"

#include <QDialog>
#include <QProcess>

class QListWidget;
class KMessageWidget;
class QPushButton;
class QHBoxLayout;
class QProgressBar;
class QGroupBox;

class WhisperDownload : public QDialog
{
    Q_OBJECT
public:
    enum WhisperRole { WPModelNameRole = Qt::UserRole, WPUrlRole, WPSizeRole };
    WhisperDownload(SpeechToText *engine, QWidget *parent = nullptr);
    static const QString getWhisperModelsFolder();
    bool newModelsInstalled();

protected:
private:
    SpeechToText *m_engine;
    QListWidget *m_lw;
    KMessageWidget *m_mw;
    QPushButton *m_bd;
    QHBoxLayout *m_downloadLayout;
    QProgressBar *m_pb;
    QGroupBox *m_downloadGroup;
    int m_downloadProgress{0};
    bool m_newModelInstalled{false};

private Q_SLOTS:
    void checkItemList();
    void parseScriptFeedback(const QString &scriptName, const QStringList args, const QStringList jobData);
    void scriptFinished(const QString &scriptName, const QStringList &args);
    void downloadModels();
    void installFeedback(const QString &feedback);
    void queryClose();

Q_SIGNALS:
    // void subtitleProgressUpdate(int);
    // void subtitleFinished(int exitCode, QProcess::ExitStatus exitStatus);
};
