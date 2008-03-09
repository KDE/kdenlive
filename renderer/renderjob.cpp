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

RenderJob::RenderJob(QString scenelist, QString dest, QString player) : QObject() {
    m_scenelist = scenelist;
    m_dest = dest;
    m_player = player;
    m_progress = 0;
    m_renderProcess = new QProcess;
    m_prog = "inigo";
    m_args << scenelist << "-consumer" << "avformat:" + m_dest << "progress=1";
    connect(m_renderProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotIsOver(int, QProcess::ExitStatus)));
    connect(m_renderProcess, SIGNAL(readyReadStandardError()), this, SLOT(receivedStderr()));
    m_renderProcess->setReadChannel(QProcess::StandardError);
}


RenderJob::~RenderJob() {
    delete m_renderProcess;
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
    QDBusInterface kuiserver("org.kde.JobViewServer", "/JobViewServer", "org.kde.JobViewServer", QDBusConnection::sessionBus());
    QDBusReply<QString> reply = kuiserver.call("requestView", "kdenlive", "kdenlive", 1);
    //kDebug()<<"///// JOB REPLY: "<<reply;
    QStringList args;
    m_jobUiserver = new QDBusInterface("org.kde.JobViewServer", "/JobViewServer/JobView_1", "org.kde.JobView", QDBusConnection::sessionBus());
    reply = m_jobUiserver->call("setInfoMessage", tr("Rendering %1").arg(m_dest));
    m_renderProcess->start(m_prog, m_args);
}


void RenderJob::slotIsOver(int exitcode, QProcess::ExitStatus status) {
    QDBusReply<QString> reply = m_jobUiserver->call("terminate", "");
    if (!m_player.isEmpty()) {
	QStringList args;
	args<<m_dest;
	QProcess::startDetached(m_player, args);
    }
    exit(1);
}

#include "renderjob.moc"
