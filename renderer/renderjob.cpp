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

#include <QFile>
#include <QStringList>
#include <QThread>
#include <QtDBus>
#include <QElapsedTimer>
#include <utility>
// Can't believe I need to do this to sleep.
class SleepThread : QThread
{
public:
    void run() override {}
    static void msleep(unsigned long msecs) { QThread::msleep(msecs); }
};

RenderJob::RenderJob(const QString &render, const QString &scenelist, const QString &target, int pid, int in, int out, QObject *parent)
    : QObject(parent)
    , m_scenelist(scenelist)
    , m_dest(std::move(target))
    , m_progress(0)
    , m_prog(std::move(render))
    , m_player()
    , m_jobUiserver(nullptr)
    , m_kdenliveinterface(nullptr)
    , m_usekuiserver(true)
    , m_logfile(target + QStringLiteral(".log"))
    , m_erase(scenelist.startsWith(QDir::tempPath()) || scenelist.startsWith(QString("xml:%2").arg(QDir::tempPath())))
    , m_seconds(0)
    , m_frame(in)
    , m_framein(in)
    , m_frameout(out)
    , m_pid(pid)
    , m_dualpass(false)
{
    m_renderProcess = new QProcess;
    m_renderProcess->setReadChannel(QProcess::StandardError);
    connect(m_renderProcess, &QProcess::stateChanged, this, &RenderJob::slotCheckProcess);

    // Disable VDPAU so that rendering will work even if there is a Kdenlive instance using VDPAU
    qputenv("MLT_NO_VDPAU", "1");
    m_args << "-progress" << scenelist;
    if (in != -1) {
        m_args << QStringLiteral("in=") + QString::number(in);
    }
    if (out != -1) {
        m_args << QStringLiteral("out=") + QString::number(out);
    }

    // Create a log of every render process.
    if (!m_logfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Unable to log to " << m_logfile.fileName();
    } else {
        m_logstream.setDevice(&m_logfile);
    }
}

RenderJob::~RenderJob()
{
    delete m_jobUiserver;
    delete m_kdenliveinterface;
    delete m_renderProcess;
    m_logfile.close();
}

void RenderJob::slotAbort(const QString &url)
{
    if (m_dest == url) {
        slotAbort();
    }
}

void RenderJob::slotAbort()
{
    qWarning() << "Job aborted by user...";
    m_renderProcess->kill();

    if (m_kdenliveinterface) {
        m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QStringLiteral("setRenderingFinished"), {m_dest, -3, QString()});
    }
    if (m_jobUiserver) {
        m_jobUiserver->call(QStringLiteral("terminate"), QString());
    }
    if (m_erase) {
        QFile(m_scenelist).remove();
    }
    QFile(m_dest).remove();
    m_logstream << "Job aborted by user" << "\n";
    m_logstream.flush();
    m_logfile.close();
    qApp->quit();
}

void RenderJob::receivedStderr()
{
    QString result = QString::fromLocal8Bit(m_renderProcess->readAllStandardError()).simplified();
    if (!result.startsWith(QLatin1String("Current Frame"))) {
        m_errorMessage.append(result + QStringLiteral("<br>"));
        m_logstream << result;
    } else {
        int progress = result.section(QLatin1Char(' '), -1).toInt();
        int frame = result.section(QLatin1Char(','), 0, 0).section(QLatin1Char(' '), -1).toInt();
        if (progress <= m_progress || progress <= 0 || progress > 100) {
            return;
        }
        m_progress = progress;
        if (m_args.contains(QStringLiteral("pass=1"))) {
            m_progress /= 2;
        } else if (m_args.contains(QStringLiteral("pass=2"))) {
            m_progress = 50 + m_progress / 2;
        }
        if ((m_kdenliveinterface != nullptr) && m_kdenliveinterface->isValid()) {
            m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QStringLiteral("setRenderingProgress"), {m_dest, m_progress, frame});
        }
        qint64 elapsedTime = m_startTime.secsTo(QDateTime::currentDateTime());
        if (elapsedTime == m_seconds) {
            return;
        }
        int speed = (frame - m_frame) / (elapsedTime - m_seconds);
        if (m_jobUiserver) {
            qint64 remaining = elapsedTime * (100 - progress) / progress;
            int days = int(remaining / 86400);
            int remainingSecs = int(remaining % 86400);
            QTime when = QTime(0, 0, 0, 0).addSecs(remainingSecs);
            QString est = tr("Remaining time ");
            if (days > 0) {
                est.append(tr("%n day(s) ", "", days));
            }
            est.append(when.toString(QStringLiteral("hh:mm:ss")));

            m_jobUiserver->call(QStringLiteral("setPercent"), uint(m_progress));
            m_jobUiserver->call(QStringLiteral("setDescriptionField"), 0, QString(), est);
            m_jobUiserver->call(QStringLiteral("setProcessedAmount"), qulonglong(frame - m_framein), tr("frames"));
            m_jobUiserver->call(QStringLiteral("setSpeed"), qulonglong(speed));
        }
        m_seconds = int(elapsedTime);
        m_frame = frame;
        m_logstream << QStringLiteral("%1\t%2\t%3\t%4\n").arg(m_seconds).arg(m_frame).arg(m_progress).arg(speed);
    }
}

void RenderJob::start()
{
    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    if ((interface != nullptr) && m_usekuiserver) {
        if (!interface->isServiceRegistered(QStringLiteral("org.kde.JobViewServer"))) {
            qWarning() << "No org.kde.JobViewServer registered, trying to start kuiserver";
            if (QProcess::startDetached(QStringLiteral("kuiserver"), QStringList())) {
                // Give it a couple of seconds to start
                QElapsedTimer t;
                t.start();
                while (!interface->isServiceRegistered(QStringLiteral("org.kde.JobViewServer")) && t.elapsed() < 3000) {
                    SleepThread::msleep(100); // Sleep 100 ms
                }
            } else {
                qWarning() << "Failed to start kuiserver";
            }
        }

        if (interface->isServiceRegistered(QStringLiteral("org.kde.JobViewServer"))) {
            QDBusInterface kuiserver(QStringLiteral("org.kde.JobViewServer"), QStringLiteral("/JobViewServer"), QStringLiteral("org.kde.JobViewServer"));
            QDBusReply<QDBusObjectPath> objectPath =
                kuiserver.asyncCall(QStringLiteral("requestView"), QLatin1String("kdenlive"), QLatin1String("kdenlive"), 0x0001);
            QString reply = QDBusObjectPath(objectPath).path();

            // Use of the KDE JobViewServer is an ugly hack, it is not reliable
            QString dbusView = QStringLiteral("org.kde.JobViewV2");
            m_jobUiserver = new QDBusInterface(QStringLiteral("org.kde.JobViewServer"), reply, dbusView);
            if ((m_jobUiserver != nullptr) && m_jobUiserver->isValid()) {
                m_startTime = QDateTime::currentDateTime();
                if (!m_args.contains(QStringLiteral("pass=2"))) {
                    m_jobUiserver->call(QStringLiteral("setPercent"), 0);
                }

                m_jobUiserver->call(QStringLiteral("setInfoMessage"), tr("Rendering %1").arg(QFileInfo(m_dest).fileName()));
                m_jobUiserver->call(QStringLiteral("setTotalAmount"), m_frameout);
                QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.JobViewServer"), reply, dbusView, QStringLiteral("cancelRequested"), this,
                                                      SLOT(slotAbort()));
            }
        }
    }
    if (m_pid > -1) {
        initKdenliveDbusInterface();
    }

    // Make sure the destination directory is writable
    /*QFileInfo checkDestination(QFileInfo(m_dest).absolutePath());
    if (!checkDestination.isWritable()) {
        slotIsOver(QProcess::NormalExit, false);
    }*/

    // Because of the logging, we connect to stderr in all cases.
    connect(m_renderProcess, &QProcess::readyReadStandardError, this, &RenderJob::receivedStderr);
    m_renderProcess->start(m_prog, m_args);
    m_logstream << "Started render process: " << m_prog << ' ' << m_args.join(QLatin1Char(' ')) << "\n";
    m_logstream.flush();
}

void RenderJob::initKdenliveDbusInterface()
{
    QString kdenliveId;
    QDBusConnection connection = QDBusConnection::sessionBus();
    QDBusConnectionInterface *ibus = connection.interface();
    kdenliveId = QStringLiteral("org.kde.kdenlive-%1").arg(m_pid);
    if (!ibus->isServiceRegistered(kdenliveId)) {
        kdenliveId.clear();
        const QStringList services = ibus->registeredServiceNames();
        for (const QString &service : services) {
            if (!service.startsWith(QLatin1String("org.kde.kdenlive"))) {
                continue;
            }
            kdenliveId = service;
            break;
        }
    }
    if (kdenliveId.isEmpty()) {
        return;
    }
    m_kdenliveinterface =
        new QDBusInterface(kdenliveId, QStringLiteral("/kdenlive/MainWindow_1"), QStringLiteral("org.kde.kdenlive.rendering"), connection, this);

    if (m_kdenliveinterface) {
        if (!m_args.contains(QStringLiteral("pass=2"))) {
            m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QStringLiteral("setRenderingProgress"), {m_dest, 0, 0});
        }
        connect(m_kdenliveinterface, SIGNAL(abortRenderJob(QString)), this, SLOT(slotAbort(QString)));
    }
}

void RenderJob::slotCheckProcess(QProcess::ProcessState state)
{
    if (state == QProcess::NotRunning) {
        slotIsOver(m_renderProcess->exitStatus());
    }
}

void RenderJob::slotIsOver(QProcess::ExitStatus status, bool isWritable)
{
    if (m_jobUiserver) {
        m_jobUiserver->call(QStringLiteral("setDescriptionField"), 1, tr("Rendered file"), m_dest);
        m_jobUiserver->call(QStringLiteral("terminate"), QString());
    }
    if (!isWritable) {
        QString error = tr("Cannot write to %1, check permissions.").arg(m_dest);
        if (m_kdenliveinterface) {
            m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QStringLiteral("setRenderingFinished"), {m_dest, -2, error});
        }
        QProcess::startDetached(QStringLiteral("kdialog"), {QStringLiteral("--error"), error});
        m_logstream << error << "\n";
        emit renderingFinished();
        //qApp->quit();
    }
    if (m_erase) {
        QFile(m_scenelist).remove();
    }
    if (status == QProcess::CrashExit || m_renderProcess->error() != QProcess::UnknownError || m_renderProcess->exitCode() != 0) {
        // rendering crashed
        if (m_kdenliveinterface) {
            m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QStringLiteral("setRenderingFinished"), {m_dest, -2, m_errorMessage});
        }
        QStringList args;
        QString error = tr("Rendering of %1 aborted, resulting video will probably be corrupted.").arg(m_dest);
        if (m_frame > 0) {
            error += QLatin1Char('\n') + tr("Frame: %1").arg(m_frame);
        }
        args << QStringLiteral("--error") << error;
        m_logstream << error << "\n";
        QProcess::startDetached(QStringLiteral("kdialog"), args);
        emit renderingFinished();
    } else {
        if (!m_dualpass && (m_kdenliveinterface != nullptr)) {
            m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QStringLiteral("setRenderingFinished"), {m_dest, -1, QString()});
        }
        m_logstream << "Rendering of " << m_dest << " finished" << "\n";
        if (!m_dualpass && m_player.length() > 3 && m_player.contains(QLatin1Char(' '))) {
            QStringList args = m_player.split(QLatin1Char(' '));
            QString exec = args.takeFirst();
            // Decode url
            QString url = QUrl::fromEncoded(args.takeLast().toUtf8()).toLocalFile();
            args << url;
            QProcess::startDetached(exec, args);
        }
        m_logstream.flush();
        if (m_dualpass) {
            deleteLater();
        } else {
            m_logfile.remove();
        }
    }
    emit renderingFinished();
}
