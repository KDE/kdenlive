/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef RENDERJOB_H
#define RENDERJOB_H

#include <QDBusInterface>
#include <QObject>
#include <QProcess>
#include <QTime>
#include <QFile>
// Testing
#include <QTextStream>

class RenderJob : public QObject
{
    Q_OBJECT

public:
    RenderJob(const QString &render, const QString &scenelist, const QString &target, int pid = -1, int in = -1, int out = -1, QObject *parent = nullptr);
    ~RenderJob();

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
    QDBusInterface *m_jobUiserver;
    QDBusInterface *m_kdenliveinterface;
    bool m_usekuiserver;
    /** @brief Used to create a temporary file for logging. */
    QFile m_logfile;
    bool m_erase;
    int m_seconds;
    int m_frame;
    /** @brief The process id of the Kdenlive instance, used to get the dbus service. */
    int m_pid;
    bool m_dualpass;
    QProcess *m_renderProcess;
    QString m_errorMessage;
    QList<QVariant> m_dbusargs;
    QTime m_startTime;
    QStringList m_args;
    /** @brief Used to write to the log file. */
    QTextStream m_logstream;
    void initKdenliveDbusInterface();

signals:
    void renderingFinished();
};

#endif
