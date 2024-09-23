/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTimer>
// Testing
#include <QTextStream>

class RenderJob : public QObject
{
    Q_OBJECT

public:
    RenderJob(const QString &render, const QString &scenelist, const QString &target, int pid = -1, int in = -1, int out = -1,
              const QString &subtitleFile = QString(), QObject *parent = nullptr);
    ~RenderJob() override;

public Q_SLOTS:
    void start();

private Q_SLOTS:
    void slotIsOver(QProcess::ExitStatus status, bool isWritable = true);
    void receivedStderr();
    void slotAbort();
    void slotAbort(const QString &url);
    void slotCheckProcess(int /*exitCode*/, QProcess::ExitStatus exitStatus);
    void slotCheckSubtitleProcess(int exitCode, QProcess::ExitStatus exitStatus);
    void receivedSubtitleProgress();

private:
    QString m_scenelist;
    QString m_dest;
    int m_progress;
    QString m_prog;
    QLocalSocket* m_kdenlivesocket;
    QTimer m_connectTimer;
    /** @brief Used to create a temporary file for logging. */
    QFile m_logfile;
    bool m_erase;
    int m_seconds;
    int m_frame;
    int m_framein;
    int m_frameout;
    /** @brief The process id of the Kdenlive instance, used to create the local socket instance. */
    int m_pid;
    bool m_dualpass;
    QString m_subtitleFile;
    QString m_temporaryRenderFile;
    QProcess *m_renderProcess;
    QProcess *m_subsProcess;
    QEventLoop m_looper;
    QString m_errorMessage;
    QDateTime m_startTime;
    QStringList m_args;
    /** @brief Used to write to the log file. */
    QTextStream m_logstream;
    QString m_outputData;
    void fromServer();
    void sendFinish(int status, const QString &error);
    void updateProgress();
    void sendProgress();

Q_SIGNALS:
    void renderingFinished();
};
