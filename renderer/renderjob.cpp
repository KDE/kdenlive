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


#include <QtDBus>

#include "renderjob.h"

static QDBusConnection connection(QLatin1String(""));

RenderJob::RenderJob(bool erase, QString renderer, QString player, QString scenelist, QString dest, QStringList args, int in, int out) : QObject() {
    m_scenelist = scenelist;
    m_dest = dest;
    m_player = player;
    m_progress = 0;
    m_erase = erase;
    m_renderProcess = new QProcess;
    m_prog = renderer;
    m_args << scenelist;
    if (in != -1) m_args << "in=" + QString::number(in);
    if (out != -1) m_args << "out=" + QString::number(out);
    m_args << "-consumer" << "avformat:" + m_dest << "progress=1" << args;
    connect(m_renderProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotIsOver(int, QProcess::ExitStatus)));
    connect(m_renderProcess, SIGNAL(readyReadStandardError()), this, SLOT(receivedStderr()));
    m_renderProcess->setReadChannel(QProcess::StandardError);
}


RenderJob::~RenderJob() {
    if (m_renderProcess) delete m_renderProcess;
}

void RenderJob::slotAbort() {
    fprintf(stderr, "Kdenlive-render: JOB ABORTED BY USER...\n");
    m_renderProcess->kill();
    QDBusReply<QString> reply = m_jobUiserver->call("terminate", "");
    if (m_erase) {
        QFile f(m_scenelist);
        f.remove();
    }
    QFile f(m_dest);
    f.remove();
    exit(1);
}

void RenderJob::receivedStderr() {
    QString result = QString(m_renderProcess->readAllStandardError());
    result = result.simplified();
    result = result.section(" ", -1);
    int pro = result.toInt();
    if (pro > m_progress) {
        m_progress = pro;
        QDBusReply<QString> reply = m_jobUiserver->call("setPercent", (uint) m_progress);
    }
}

void RenderJob::start() {
    QDBusInterface kuiserver("org.kde.JobViewServer", "/JobViewServer", "org.kde.JobViewServer");
    QDBusReply<QDBusObjectPath> objectPath = kuiserver.call("requestView", "kdenlive", "kdenlive", 1);
    QString reply = ((QDBusObjectPath) objectPath).path();
    m_jobUiserver = new QDBusInterface("org.kde.JobViewServer", reply, "org.kde.JobView");
    m_jobUiserver->call("setInfoMessage", tr("Rendering %1").arg(m_dest));

    QDBusConnection::sessionBus().connect("org.kde.JobViewServer", reply, "org.kde.JobView",
                                          "cancelRequested", this, SLOT(slotAbort()));
    m_renderProcess->start(m_prog, m_args);
}


void RenderJob::slotIsOver(int exitcode, QProcess::ExitStatus status) {
    QDBusReply<QString> reply = m_jobUiserver->call("terminate", "");
    if (m_erase) {
        QFile f(m_scenelist);
        f.remove();
    }
    if (m_player != "-") {
        QStringList args;
        args << m_dest;
        QProcess::startDetached(m_player, args);
    }
    exit(1);
}

#include "renderjob.moc"
