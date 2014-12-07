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


#include "renderjob.h"

#include <QtDBus>
#include <QFile>
#include <QThread>
#include <QStringList>


// Can't believe I need to do this to sleep.
class SleepThread : QThread
{
public:
    virtual void run() {}
    static void msleep(unsigned long msecs) {
        QThread::msleep(msecs);
    }
};


RenderJob::RenderJob(bool erase, bool usekuiserver, int pid, const QString& renderer, const QString& profile, const QString& rendermodule, const QString& player, const QString& scenelist, const QString& dest, const QStringList& preargs, const QStringList& args, int in, int out) :
    QObject(),
    m_jobUiserver(NULL),
    m_kdenliveinterface(NULL),
    m_usekuiserver(usekuiserver),
    m_enablelog(false),
    m_pid(pid)
{
    m_scenelist = scenelist;
    m_dest = dest;
    m_player = player;
    m_progress = 0;
    m_erase = erase;
    m_renderProcess = new QProcess;
    
    // Disable VDPAU so that rendering will work even if there is a Kdenlive instance using VDPAU
#if QT_VERSION >= 0x040600
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QLatin1String("MLT_NO_VDPAU"), QLatin1String("1"));
    m_renderProcess->setProcessEnvironment(env);
#else
    QStringList env = QProcess::systemEnvironment();
    env << QLatin1String("MLT_NO_VDPAU=1");
    m_renderProcess->setEnvironment(env);
#endif

    m_prog = renderer;
    m_args << scenelist;
    if (in != -1) m_args << QLatin1String("in=") + QString::number(in);
    if (out != -1) m_args << QLatin1String("out=") + QString::number(out);

    m_args << preargs;
    //qDebug()<<"PRE ARGS: "<<preargs;
    if (scenelist.startsWith(QLatin1String("consumer:"))) {
        // Use MLT's producer_consumer, needs a different syntax for profile:
        m_args << QLatin1String("profile=") + profile;
    } else m_args << QLatin1String("-profile") << profile;
    m_args << QLatin1String("-consumer") << rendermodule + QLatin1Char(':') + m_dest << QLatin1String("progress=1") << args;

    m_dualpass = false;
    if (args.contains(QLatin1String("pass=1"))) m_dualpass = true;

    connect(m_renderProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(slotCheckProcess(QProcess::ProcessState)));
    m_renderProcess->setReadChannel(QProcess::StandardError);

    m_enablelog = (!qgetenv("KDENLIVE_RENDER_LOG").isEmpty());
    if (m_enablelog) {
        // Create a log of every render process.
        m_logfile.setAutoRemove(false);
        m_logfile.setFileTemplate(QDir::tempPath() + QLatin1String("/kdenlive_render.log.XXXXXXXX"));
        if (m_logfile.open()) {
            qDebug() << "Writing render log to " << m_logfile.fileName();

        } else {
            qDebug() << "Unable to log to " << m_logfile.fileName();
        }
        m_logstream.setDevice(&m_logfile);
        QString tmplist = scenelist;
        if (tmplist.contains(QLatin1String("consumer:"))) {
            const QStringList tl = tmplist.split(QLatin1String("consumer:"));
            if (tl.count() == 2) {
                tmplist = tl[1];
            }
        }
        m_logstream << "Log starting. Dumping contents of " << tmplist << endl;
        QFile file(tmplist);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_logstream << "Unable to read contents of " << tmplist << endl;
        } else {
            m_logstream.flush();
            QTextStream in(&file);
            m_logstream << in.readAll() << endl;
        }
    }
}


RenderJob::~RenderJob()
{
    delete m_renderProcess;
    if (m_enablelog) {
        m_logfile.close();
    }
}

void RenderJob::setLocale(const QString &locale)
{
#if QT_VERSION >= 0x040600
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QLatin1String("LC_NUMERIC"), locale);
    m_renderProcess->setProcessEnvironment(env);
#else
    QStringList env = QProcess::systemEnvironment();
    env << QString::fromLatin1("LC_NUMERIC=%1").arg(locale);
    m_renderProcess->setEnvironment(env);
#endif
}

void RenderJob::slotAbort(const QString& url)
{
    if (m_dest == url) slotAbort();
}

void RenderJob::slotAbort()
{
    qDebug() << "Kdenlive-render: JOB ABORTED BY USER...";
    m_renderProcess->kill();

    if (m_kdenliveinterface) {
        m_dbusargs[1] = -3;
        m_dbusargs.append(QString());
        m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QLatin1String("setRenderingFinished"), m_dbusargs);
    }
    if (m_jobUiserver) m_jobUiserver->call(QLatin1String("terminate"), QString());
    if (m_erase) {
        QFile f(m_scenelist);
        f.remove();
    }
    QFile f(m_dest);
    f.remove();
    if (m_enablelog) {
        m_logstream << "Job aborted by user" << endl;
        m_logstream.flush();
        m_logfile.close();
    }
    qApp->quit();
}

void RenderJob::receivedStderr()
{
    QString result = QString::fromLocal8Bit(m_renderProcess->readAllStandardError()).simplified();
    //fprintf(stderr, "* * * *RENDER LG: %s\n", result.toUtf8().data());
    if (!result.startsWith(QLatin1String("Current Frame"))) m_errorMessage.append(result + QLatin1String("<br>"));
    else {
        if (m_enablelog) m_logstream << "ReceivedStderr from melt: " << result << endl;
        result = result.section(QLatin1Char(' '), -1);
        int pro = result.toInt();
        if (pro < 0 || pro > 100) return;
        if (pro > m_progress) {
            m_progress = pro;
            if (m_args.contains(QLatin1String("pass=1"))) {
                m_progress /= 2.0;
            } else if (m_args.contains(QLatin1String("pass=2"))) {
                m_progress = 50 + m_progress / 2.0;
            }
            if (m_kdenliveinterface) {
                if (!m_kdenliveinterface->isValid()) {
                    delete m_kdenliveinterface;
                    m_kdenliveinterface = NULL;
                    // qDebug() << "BROKEN COMMUNICATION WITH KDENLIVE";
                } else {
                    m_dbusargs[1] = m_progress;
                    m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QLatin1String("setRenderingProgress"), m_dbusargs);
                }
            } else if (pro % 5 == 0) {
                // Try to restart communication with Kdenlive every 5 percents
                // qDebug() << "TRYING TO RESTART COMMUNICATION WITH KDENLIVE";
                initKdenliveDbusInterface();
            }

            if (m_jobUiserver) {
                m_jobUiserver->call(QLatin1String("setPercent"), (uint) m_progress);
                /*int seconds = m_startTime.secsTo(QTime::currentTime());
                seconds = seconds * (100 - m_progress) / m_progress;
                m_jobUiserver->call("setDescriptionField", (uint) 1, tr("Remaining time"), QTime().addSecs(seconds).toString("hh:mm:ss"));*/
            }
        }
    }
}

void RenderJob::start()
{
    QDBusConnectionInterface* interface = QDBusConnection::sessionBus().interface();
    if (interface && m_usekuiserver) {
        if (!interface->isServiceRegistered(QLatin1String("org.kde.JobViewServer"))) {
            qDebug() << "No org.kde.JobViewServer registered, trying to start kuiserver";
            if (m_enablelog) m_logstream << "No org.kde.JobViewServer registered, trying to start kuiserver";
            if (QProcess::startDetached(QLatin1String("kuiserver"))) {
                qDebug() << "Started kuiserver";
                if (m_enablelog) m_logstream << "Started kuiserver";
                // Give it a couple of seconds to start
                QTime t;
                t.start();
                while (!interface->isServiceRegistered(QLatin1String("org.kde.JobViewServer")) && t.elapsed() < 3000) {
                    SleepThread::msleep(100);   //Sleep 100 ms
                }
            } else {
                qDebug() << "Failed to start kuiserver";
                if (m_enablelog) m_logstream << "Failed to start kuiserver";
            }
        }

        if (interface->isServiceRegistered(QLatin1String("org.kde.JobViewServer"))) {
            QDBusInterface kuiserver(QLatin1String("org.kde.JobViewServer"), QLatin1String("/JobViewServer"), QLatin1String("org.kde.JobViewServer"));
            QDBusReply<QDBusObjectPath> objectPath = kuiserver.call(QLatin1String("requestView"),QLatin1String("Kdenlive"), QLatin1String("kdenlive"), 0x0001);
            QString reply = ((QDBusObjectPath) objectPath).path();

            // Use of the KDE JobViewServer is an ugly hack, it is not reliable
            QString dbusView = QLatin1String("org.kde.JobViewV2");
            m_jobUiserver = new QDBusInterface(QLatin1String("org.kde.JobViewServer"), reply, dbusView);
            if (!m_jobUiserver || !m_jobUiserver->isValid()) {
                dbusView = QLatin1String("org.kde.JobView");
                m_jobUiserver = new QDBusInterface(QLatin1String("org.kde.JobViewServer"), reply, dbusView);
            }
                
            if (m_jobUiserver && m_jobUiserver->isValid()) {
                m_startTime = QTime::currentTime();
                if (!m_args.contains(QLatin1String("pass=2")))
                    m_jobUiserver->call(QLatin1String("setPercent"), (uint) 0);
                //m_jobUiserver->call("setInfoMessage", tr("Rendering %1").arg(QFileInfo(m_dest).fileName()));
                m_jobUiserver->call(QLatin1String("setDescriptionField"), (uint) 0, tr("Rendering"), m_dest);
                QDBusConnection::sessionBus().connect(QLatin1String("org.kde.JobViewServer"), reply, dbusView, QLatin1String("cancelRequested"), this, SLOT(slotAbort()));
            }
        }
    }

    initKdenliveDbusInterface();
    
    // Make sure the destination file is writable
    QFile checkDestination(m_dest);
    if (!checkDestination.open(QIODevice::WriteOnly)) {
        slotIsOver(QProcess::NormalExit, false);
    }
    checkDestination.close();

    // Because of the logging, we connect to stderr in all cases.
    connect(m_renderProcess, SIGNAL(readyReadStandardError()), this, SLOT(receivedStderr()));
    m_renderProcess->start(m_prog, m_args);
    if (m_enablelog) m_logstream << "Started render process: " << m_prog << ' ' << m_args.join(QLatin1String(" ")) << endl;
    qDebug() << "Started render process: " << m_prog << ' ' << m_args.join(QLatin1String(" "));
}


void RenderJob::initKdenliveDbusInterface()
{
    QString kdenliveId;
    QDBusConnection connection = QDBusConnection::sessionBus();
    QDBusConnectionInterface* ibus = connection.interface();
    kdenliveId = QString::fromLatin1("org.kde.kdenlive-%1").arg(m_pid);
    if (!ibus->isServiceRegistered(kdenliveId))
    {
        kdenliveId.clear();
        const QStringList services = ibus->registeredServiceNames();
        foreach(const QString & service, services) {
            if (!service.startsWith(QLatin1String("org.kde.kdenlive")))
                continue;
            kdenliveId = service;
            break;
        }
    }
    m_dbusargs.clear();
    if (kdenliveId.isEmpty()) return;
    m_kdenliveinterface = new QDBusInterface(kdenliveId,
            QLatin1String("/kdenlive/MainWindow_1"),
            QLatin1String("org.kdenlive.MainWindow"),
            connection,
            this);

    if (m_kdenliveinterface) {
        m_dbusargs.append(m_dest);
        m_dbusargs.append((int) 0);
        if (!m_args.contains(QLatin1String("pass=2")))
            m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QLatin1String("setRenderingProgress"), m_dbusargs);
        connect(m_kdenliveinterface, SIGNAL(abortRenderJob(QString)),
                this, SLOT(slotAbort(QString)));
    }
}


void RenderJob::slotCheckProcess(QProcess::ProcessState state)
{
    if (state == QProcess::NotRunning) slotIsOver(m_renderProcess->exitStatus());
}



void RenderJob::slotIsOver(QProcess::ExitStatus status, bool isWritable)
{
    if (m_jobUiserver) m_jobUiserver->call(QLatin1String("terminate"), QString());
    if (!isWritable) {
        QString error = tr("Cannot write to %1, check the permissions.").arg(m_dest);
        if (m_kdenliveinterface) {
            m_dbusargs[1] = (int) - 2;
            m_dbusargs.append(error);
            m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QLatin1String("setRenderingFinished"), m_dbusargs);
        }
        QStringList args;
        args << QLatin1String("--error") << error;
        if (m_enablelog) m_logstream << error << endl;
        qDebug() << error;
        QProcess::startDetached(QLatin1String("kdialog"), args);
        qApp->quit();
    }
    if (m_erase) {
        QFile f(m_scenelist);
        f.remove();
    }
    if (status == QProcess::CrashExit || m_renderProcess->error() != QProcess::UnknownError) {
        // rendering crashed
        if (m_kdenliveinterface) {
            m_dbusargs[1] = (int) - 2;
            m_dbusargs.append(m_errorMessage);
            m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QLatin1String("setRenderingFinished"), m_dbusargs);
        }
        QStringList args;
        QString error = tr("Rendering of %1 aborted, resulting video will probably be corrupted.").arg(m_dest);
        args << QLatin1String("--error") << error;
        if (m_enablelog) m_logstream << error << endl;
        qDebug() << error;
        QProcess::startDetached(QLatin1String("kdialog"), args);
        qApp->quit();
    } else {
        if (!m_dualpass && m_kdenliveinterface) {
            m_dbusargs[1] = (int) - 1;
            m_dbusargs.append(QString());
            m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QLatin1String("setRenderingFinished"), m_dbusargs);
        }
        if (m_enablelog) m_logstream << "Rendering of " << m_dest << " finished" << endl;
        qDebug() << "Rendering of " << m_dest << " finished";
        if (!m_dualpass && m_player != QLatin1String("-")) {
            if (m_enablelog) m_logstream << "Starting player" << endl;
            QStringList args;
            args << m_dest;
            QProcess::startDetached(m_player, args);
        }
        if (m_dualpass) {
            emit renderingFinished();
            deleteLater();
        } else qApp->quit();
    }
}

#include "renderjob.moc"
