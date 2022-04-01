/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#ifdef NODBUS
#include <QLocalSocket>
#else
#include <QDBusInterface>
#endif
#include <QObject>
#include <QProcess>
#include <QDateTime>
#include <QFile>
// Testing
#include <QTextStream>

class RenderJob : public QObject
{
    Q_OBJECT

public:
    RenderJob(const QString &render, const QString &scenelist, const QString &target, int pid = -1, int in = -1, int out = -1, QObject *parent = nullptr);
    ~RenderJob() override;

public slots:
    void start();

private slots:
    void slotIsOver(QProcess::ExitStatus status, bool isWritable = true);
    void receivedStderr();
    void slotAbort();
    void slotAbort(const QString &url);
    void slotCheckProcess(QProcess::ProcessState state);

private:
    QString m_scenelist;
    QString m_dest;
    int m_progress;
    QString m_prog;
    QString m_player;
#ifdef NODBUS
    QLocalSocket* m_kdenlivesocket;
#else
    QDBusInterface *m_jobUiserver;
    QDBusInterface *m_kdenliveinterface;
#endif
    bool m_usekuiserver;
    /** @brief Used to create a temporary file for logging. */
    QFile m_logfile;
    bool m_erase;
    int m_seconds;
    int m_frame;
    int m_framein;
    int m_frameout;
    /** @brief The process id of the Kdenlive instance, used to get the dbus service. */
    int m_pid;
    bool m_dualpass;
    QProcess *m_renderProcess;
    QString m_errorMessage;
    QList<QVariant> m_dbusargs;
    QDateTime m_startTime;
    QStringList m_args;
    /** @brief Used to write to the log file. */
    QTextStream m_logstream;
#ifdef NODBUS
    void fromServer();
#else
    void initKdenliveDbusInterface();
#endif
    void sendFinish(int status, const QString &error);
    void sendProgress();

signals:
    void renderingFinished();
};
