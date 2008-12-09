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
#include <QThread>
#include "renderjob.h"

// Can't believe I need to do this to sleep.   
   class SleepThread : QThread {   
     public:   
     virtual void run() {};   
     static void msleep(unsigned long msecs) {   
       QThread::msleep(msecs);   
     }   
   };

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
    m_renderProcess->setReadChannel(QProcess::StandardError);
}


RenderJob::~RenderJob() {
    if (m_renderProcess) delete m_renderProcess;
}

void RenderJob::slotAbort() {
    qDebug() << "Kdenlive-render: JOB ABORTED BY USER...";
    m_renderProcess->kill();
    if (m_jobUiserver) m_jobUiserver->call("terminate", QString());
    if (m_erase) {
        QFile f(m_scenelist);
        f.remove();
    }
    QFile f(m_dest);
    f.remove();
    qApp->quit();
}

void RenderJob::receivedStderr() {
    QString result = QString(m_renderProcess->readAllStandardError()).simplified();
    result = result.section(" ", -1);
    int pro = result.toInt();
    if (pro > m_progress) {
        m_progress = pro;
	m_jobUiserver->call("setPercent", (uint) m_progress);
	int seconds = m_startTime.secsTo(QTime::currentTime());
	seconds = seconds * (100 - m_progress) / m_progress;
	m_jobUiserver->call("setDescriptionField", (uint) 1, tr("Remaining time"), QTime(0, 0, seconds).toString("hh:mm:ss"));
    }
}

void RenderJob::start() {
    QDBusConnectionInterface* interface = QDBusConnection::sessionBus().interface();
    if (interface) {
	if (!interface->isServiceRegistered("org.kde.JobViewServer")) {
	    qDebug() << "No org.kde.JobViewServer registered, trying to start kuiserver"; 
	    if ( QProcess::startDetached( "kuiserver" ) ) {
		qDebug() << "Started kuiserver";
		// Give it a couple of seconds to start
		QTime t;
		t.start();
		while ( !interface->isServiceRegistered("org.kde.JobViewServer") && t.elapsed() < 3000 ) {
		    SleepThread::msleep( 100 ); //Sleep 100 ms
		}
	    } else {
		qDebug() << "Failed to start kuiserver"; 
	    }
	}
    
	if (interface->isServiceRegistered("org.kde.JobViewServer")) {
	    QDBusInterface kuiserver("org.kde.JobViewServer", "/JobViewServer", "org.kde.JobViewServer");
	    QDBusReply<QDBusObjectPath> objectPath = kuiserver.call("requestView", "kdenlive", "kdenlive", 1);
	    QString reply = ((QDBusObjectPath) objectPath).path();
	    m_jobUiserver = new QDBusInterface("org.kde.JobViewServer", reply, "org.kde.JobView");
	    if (m_jobUiserver) {
		m_startTime = QTime::currentTime();
		m_jobUiserver->call("setPercent", (uint) 0);
		QDBusReply<QString> reply = m_jobUiserver->call("setInfoMessage", tr("Rendering %1").arg(QFileInfo(m_dest).fileName()));
		m_jobUiserver->call("setDescriptionField", (uint) 0, tr("Rendering to"), m_dest);
		QDBusConnection::sessionBus().connect("org.kde.JobViewServer", reply, "org.kde.JobView", "cancelRequested", this, SLOT(slotAbort()));
		connect(m_renderProcess, SIGNAL(readyReadStandardError()), this, SLOT(receivedStderr()));
	    }
	}
    }
    m_renderProcess->start(m_prog, m_args);
}


void RenderJob::slotIsOver(int exitcode, QProcess::ExitStatus status) {
    if (m_jobUiserver) m_jobUiserver->call("terminate", QString());
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
	if (interface && interface->isServiceRegistered("org.kde.knotify")) {
	    QDBusMessage m = QDBusMessage::createMethodCall("org.kde.knotify",
                             "/Notify",
                             "org.kde.KNotify",
                             "event");
	    int seconds = m_startTime.secsTo(QTime::currentTime());
	    QList<QVariant> args;
	    args.append(QString("RenderFinished"));   // action name
	    args.append(QString("kdenlive"));   // app name
	    args.append(QVariantList());   // contexts
	    args.append(tr("Rendering of %1 finished in %2").arg(m_dest, QTime(0, 0, seconds).toString("hh:mm:ss")));   // body
	    args.append(QByteArray());   // app icon
	    QStringList actionList;
	    args.append(actionList);   // actions
	    qlonglong wid;
	    args.append(wid);   // win id

	    m.setArguments(args);
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
