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
#include <QFile>

#include "renderjob.h"

static QDBusConnection connection(QLatin1String(""));

RenderJob::RenderJob(bool erase, const QString &renderer, const QString &profile, const QString &rendermodule, const QString &player, const QString &scenelist, const QString &dest, const QStringList &preargs, const QStringList &args, int in, int out) : QObject(), m_jobUiserver(NULL) {
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
    m_args << preargs;
    //qDebug()<<"PRE ARGS: "<<preargs;
    m_args << "-profile" << profile;
    m_args << "-consumer" << rendermodule + ":" + m_dest << "progress=1" << args;
    connect(m_renderProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotIsOver(int, QProcess::ExitStatus)));
    connect(m_renderProcess, SIGNAL(readyReadStandardError()), this, SLOT(receivedStderr()));
    m_renderProcess->setReadChannel(QProcess::StandardError);
}


RenderJob::~RenderJob() {
    if (m_renderProcess) delete m_renderProcess;
}

void RenderJob::slotAbort() {
    qDebug() << "Kdenlive-render: JOB ABORTED BY USER...";
    m_renderProcess->kill();
    if (m_jobUiserver) QDBusReply<QString> reply = m_jobUiserver->call("terminate", "");
    if (m_erase) {
        QFile f(m_scenelist);
        f.remove();
    }
    QFile f(m_dest);
    f.remove();
    qApp->quit();
}

void RenderJob::receivedStderr() {
    QString result = QString(m_renderProcess->readAllStandardError());
    result = result.simplified();
    result = result.section(" ", -1);
    int pro = result.toInt();
    if (pro > m_progress) {
        m_progress = pro;
        if (m_jobUiserver) QDBusReply<QString> reply = m_jobUiserver->call("setPercent", (uint) m_progress);
    }
}

void RenderJob::start() {
    QDBusConnectionInterface* interface = QDBusConnection::sessionBus().interface();
    if (interface && interface->isServiceRegistered("org.kde.JobViewServer")) {
	QDBusInterface kuiserver("org.kde.JobViewServer", "/JobViewServer", "org.kde.JobViewServer");
	QDBusReply<QDBusObjectPath> objectPath = kuiserver.call("requestView", "kdenlive", "kdenlive", 1);
	QString reply = ((QDBusObjectPath) objectPath).path();
	m_jobUiserver = new QDBusInterface("org.kde.JobViewServer", reply, "org.kde.JobView");
	m_jobUiserver->call("setInfoMessage", tr("Rendering %1").arg(m_dest.section('/', -1)));

	QDBusConnection::sessionBus().connect("org.kde.JobViewServer", reply, "org.kde.JobView",
                                          "cancelRequested", this, SLOT(slotAbort()));
    }
    m_renderProcess->start(m_prog, m_args);
}


void RenderJob::slotIsOver(int exitcode, QProcess::ExitStatus status) {
    if (m_jobUiserver) QDBusReply<QString> reply = m_jobUiserver->call("terminate", "");
    if (m_erase) {
        QFile f(m_scenelist);
        f.remove();
    }
    if (status == QProcess::CrashExit) {
        // rendering crashed
        QStringList args;
        args << "--error" << tr("Rendering of %1 aborted, resulting video will probably be corrupted.").arg(m_dest);
        QProcess::startDetached("kdialog", args);
    } else {
	QDBusConnectionInterface* interface = QDBusConnection::sessionBus().interface();
        if (interface && interface->isServiceRegistered("org.kde.VisualNotifications")) {
			QDBusMessage m = QDBusMessage::createMethodCall("org.kde.VisualNotifications",
                                              "/VisualNotifications",
                                              "org.kde.VisualNotifications",
                                              "Notify");
			QList<QVariant> args;
			uint id = 0;
			int timeout = 5000;
			args.append( QString("kdenlive") ); // app_name
			args.append( id ); // replaces_id
			args.append( QString("kdenlive") ); // app_icon
			args.append( tr("Rendering finished")); // summary
			args.append( tr("Rendering of %1 is over").arg(m_dest) ); // body
			QStringList actionList;
			args.append( actionList ); // actions
			args.append( QVariantMap() ); // hints - unused atm
			args.append( timeout ); // expire timout

			m.setArguments( args );
			QDBusMessage replyMsg = QDBusConnection::sessionBus().call(m);
		}

		if (m_player != "-") {
			QStringList args;
			args << m_dest;
			QProcess::startDetached(m_player, args);
		}
    }
    qApp->quit();
}

#include "renderjob.moc"
