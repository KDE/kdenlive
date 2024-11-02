/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "pythoninterfaces/speechtotext.h"

#include <QDialog>
#include <QMutex>
#include <QProcess>

class QProgressBar;
class QToolButton;
class QGroupBox;

class ModelDownloadWidget : public QWidget
{
    Q_OBJECT
public:
    enum WhisperRole { WPModelNameRole = Qt::UserRole, WPUrlRole, WPSizeRole, WPInstalledRole };
    ModelDownloadWidget(SpeechToText *engine, const QString &scriptPath, const QStringList &args, QWidget *parent = nullptr);
    virtual ~ModelDownloadWidget();
    void startDownload();

private:
    SpeechToText *m_engine;
    QProgressBar *m_pb;
    QToolButton *m_tb;
    QString m_scriptPath;
    QStringList m_args;
    int m_downloadProgress{-1};

private Q_SLOTS:
    void processDownload();

Q_SIGNALS:
    void abortScript();
    void jobDone(bool success);
    void jobSuccess(const QString &);
    void installFeedback(const QString &);
};
