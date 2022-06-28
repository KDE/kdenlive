/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderjob.h"

#include <QStringList>
#include <QThread>
#ifndef NODBUS
#include <QtDBus>
#else
#include <QJsonDocument>
#include <QJsonObject>
#endif
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <utility>
// Can't believe I need to do this to sleep.
class SleepThread : QThread
{
public:
    void run() override {}
    static void msleep(unsigned long msecs) { QThread::msleep(msecs); }
};

RenderJob::RenderJob(const QString &render, const QString &scenelist, const QString &target, int pid, int in, int out, const QString &subtitleFile,
                     QObject *parent)
    : QObject(parent)
    , m_scenelist(scenelist)
    , m_dest(target)
    , m_progress(0)
    , m_prog(render)
    , m_player()
#ifndef NODBUS
    , m_jobUiserver(nullptr)
    , m_kdenliveinterface(nullptr)
#else
    , m_kdenlivesocket(new QLocalSocket(this))
#endif
    , m_usekuiserver(true)
    , m_logfile(m_dest + QStringLiteral(".log"))
    , m_erase(scenelist.startsWith(QDir::tempPath()) || scenelist.startsWith(QString("xml:%2").arg(QDir::tempPath())))
    , m_seconds(0)
    , m_frame(in)
    , m_framein(in)
    , m_frameout(out)
    , m_pid(pid)
    , m_dualpass(false)
    , m_subtitleFile(subtitleFile)
{
    m_renderProcess = new QProcess(&m_looper);
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
#ifndef NODBUS
    delete m_jobUiserver;
    delete m_kdenliveinterface;
#else
    if (m_kdenlivesocket->state() == QLocalSocket::ConnectedState) {
        m_kdenlivesocket->disconnectFromServer();
    }
    delete m_kdenlivesocket;
#endif
    delete m_renderProcess;
    m_logfile.close();
}

void RenderJob::slotAbort(const QString &url)
{
    if (m_dest == url) {
        slotAbort();
    }
}

void RenderJob::sendFinish(int status, const QString &error)
{
#ifndef NODBUS
    if (m_kdenliveinterface) {
        m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QStringLiteral("setRenderingFinished"), {m_dest, status, error});
    }
    if (m_jobUiserver) {
        if (status > -3) {
            m_jobUiserver->call(QStringLiteral("setDescriptionField"), 1, tr("Rendered file"), m_dest);
        }
        m_jobUiserver->call(QStringLiteral("terminate"), QString());
    }
#else
    QJsonObject method, args;
    args["url"] = m_dest;
    args["status"] = status;
    args["error"] = error;
    method["setRenderingFinished"] = args;
    m_kdenlivesocket->write(QJsonDocument(method).toJson());
    m_kdenlivesocket->flush();
#endif
}

void RenderJob::slotAbort()
{
    m_renderProcess->kill();
    sendFinish(-3, QString());
    if (m_erase) {
        QFile(m_scenelist).remove();
    }
    QFile(m_dest).remove();
    m_logstream << "Job aborted by user"
                << "\n";
    m_logstream.flush();
    m_logfile.close();
#ifndef NODBUS
    qApp->quit();
#endif
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
        qint64 elapsedTime = m_startTime.secsTo(QDateTime::currentDateTime());
        if (elapsedTime == m_seconds) {
            return;
        }
        int speed = (frame - m_frame) / (elapsedTime - m_seconds);
        m_seconds = elapsedTime;
        m_frame = frame;
        updateProgress(speed);
    }
}

void RenderJob::updateProgress(int speed)
{
#ifndef NODBUS
    if ((m_kdenliveinterface != nullptr) && m_kdenliveinterface->isValid()) {
        m_kdenliveinterface->callWithArgumentList(QDBus::NoBlock, QStringLiteral("setRenderingProgress"), {m_dest, m_progress, m_frame});
    }
    if (m_jobUiserver) {
        qint64 remaining = m_seconds * (100 - m_progress) / m_progress;
        int days = int(remaining / 86400);
        int remainingSecs = int(remaining % 86400);
        QTime when = QTime(0, 0, 0, 0).addSecs(remainingSecs);
        QString est = tr("Remaining time ");
        if (days > 0) {
            est.append(tr("%n day(s) ", "", days));
        }
        est.append(when.toString(QStringLiteral("hh:mm:ss")));
        m_jobUiserver->call(QStringLiteral("setPercent"), uint(m_progress));
        m_jobUiserver->call(QStringLiteral("setProcessedAmount"), qulonglong(m_frame - m_framein), tr("frames"));
        if (speed > -1) {
            m_jobUiserver->call(QStringLiteral("setSpeed"), qulonglong(speed));
        }
        m_jobUiserver->call(QStringLiteral("setDescriptionField"), 0, QString(), est);
    }
#else
    QJsonObject method, args;
    args["url"] = m_dest;
    args["progress"] = m_progress;
    args["frame"] = m_frame;
    method["setRenderingProgress"] = args;
    m_kdenlivesocket->write(QJsonDocument(method).toJson());
    m_kdenlivesocket->flush();
#endif
    m_logstream << QStringLiteral("%1\t%2\t%3\n").arg(m_seconds).arg(m_frame).arg(m_progress);
}

void RenderJob::start()
{
    m_startTime = QDateTime::currentDateTime();
#ifndef NODBUS
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
#else
    connect(m_kdenlivesocket, &QLocalSocket::connected, this, [this]() {
        m_kdenlivesocket->write(QJsonDocument({{"url", m_dest}}).toJson());
        m_kdenlivesocket->flush();
        QJsonObject method, args;
        args["url"] = m_dest;
        args["progress"] = 0;
        args["frame"] = 0;
        method["setRenderingProgress"] = args;
        m_kdenlivesocket->write(QJsonDocument(method).toJson());
        m_kdenlivesocket->flush();
    });
    connect(m_kdenlivesocket, &QLocalSocket::readyRead, this, [this]() {
        QByteArray msg = m_kdenlivesocket->readAll();
        if (msg == "abort") {
            slotAbort();
        }
    });
    QString servername = QStringLiteral("org.kde.kdenlive-%1").arg(m_pid);
    m_kdenlivesocket->connectToServer(servername);
#endif

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
    m_looper.exec();
}

#ifndef NODBUS
void RenderJob::initKdenliveDbusInterface()
{
    QString kdenliveId = QStringLiteral("org.kde.kdenlive-%1").arg(m_pid);
    QDBusConnection connection = QDBusConnection::sessionBus();
    QDBusConnectionInterface *ibus = connection.interface();
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
#endif

void RenderJob::slotCheckProcess(QProcess::ProcessState state)
{
    if (state == QProcess::NotRunning) {
        slotIsOver(m_renderProcess->exitStatus());
    }
}

void RenderJob::slotIsOver(QProcess::ExitStatus status, bool isWritable)
{
    if (!isWritable) {
        QString error = tr("Cannot write to %1, check permissions.").arg(m_dest);
        sendFinish(-2, error);
        // assumes kdialog installed!!
        QProcess::startDetached(QStringLiteral("kdialog"), {QStringLiteral("--error"), error});
        m_logstream << error << "\n";
        emit renderingFinished();
        // qApp->quit();
    }
    if (m_erase) {
        QFile(m_scenelist).remove();
    }
    if (status == QProcess::CrashExit || m_renderProcess->error() != QProcess::UnknownError || m_renderProcess->exitCode() != 0) {
        // rendering crashed
        sendFinish(-2, m_errorMessage);
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
        m_logstream << "Rendering of " << m_dest << " finished"
                    << "\n";
        /*
            // Deprecated, we now play the rendering from Kdenlive's main application
            if (!m_dualpass && m_player.length() > 3 && m_player.contains(QLatin1Char(' '))) {
            QStringList args = m_player.split(QLatin1Char(' '));
            QString exec = args.takeFirst();
            // Decode url
            QString url = QUrl::fromEncoded(args.takeLast().toUtf8()).toLocalFile();
            args << url;
            QProcess::startDetached(exec, args);
            }*/

        m_logstream.flush();
        if (m_dualpass) {
            deleteLater();
        } else {
            m_logfile.remove();
            if (!m_subtitleFile.isEmpty()) {
                // Embed subtitles
                QString ffmpegExe = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
                if (!ffmpegExe.isEmpty()) {
                    QFileInfo videoRender(m_dest);
                    m_temporaryRenderFile = QDir::temp().absoluteFilePath(videoRender.fileName());
                    QStringList args = {
                        "-y", "-v", "quiet", "-stats", "-i", m_dest, "-i", m_subtitleFile, "-c", "copy", "-f", "matroska", m_temporaryRenderFile};
                    qDebug() << "::: JOB ARGS: " << args;
                    m_progress = 0;
                    disconnect(m_renderProcess, &QProcess::stateChanged, this, &RenderJob::slotCheckProcess);
                    disconnect(m_renderProcess, &QProcess::readyReadStandardError, this, &RenderJob::receivedStderr);
                    m_subsProcess = new QProcess(&m_looper);
                    m_subsProcess->setProcessChannelMode(QProcess::MergedChannels);
                    connect(m_subsProcess, &QProcess::readyReadStandardOutput, this, &RenderJob::receivedSubtitleProgress);
                    m_subsProcess->start(ffmpegExe, args);
                    m_subsProcess->waitForStarted(-1);
                    m_subsProcess->waitForFinished(-1);
                    slotCheckSubtitleProcess(m_subsProcess->exitCode(), m_subsProcess->exitStatus());
                    return;
                }
            }
            sendFinish(-1, QString());
        }
    }
    emit renderingFinished();
    m_looper.quit();
}

void RenderJob::receivedSubtitleProgress()
{
    QString outputData = QString::fromLocal8Bit(m_subsProcess->readAllStandardOutput()).simplified();
    if (outputData.isEmpty()) {
        return;
    }
    QStringList output = outputData.split(QLatin1Char(' '));
    m_errorMessage.append(outputData + QStringLiteral("<br>"));
    QString result = output.takeFirst();
    bool ok = false;
    int frame = -1;
    if (result == (QLatin1String("frame=")) && !output.isEmpty()) {
        // Frame number is the second parameter
        result = output.takeFirst();
        frame = result.toInt(&ok);
    } else if (result.startsWith(QLatin1String("frame="))) {
        frame = result.section(QLatin1Char('='), 1).toInt(&ok);
    }
    if (ok && frame > 0) {
        m_frame = frame;
        m_progress = 100 * frame / (m_frameout - m_framein);
        if (m_progress > 0) {
            updateProgress();
        }
    }
}

void RenderJob::slotCheckSubtitleProcess(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit || !QFile::exists(m_temporaryRenderFile)) {
        // rendering crashed
        qDebug() << ":::: FOUND ERROR IN SUBS: " << exitStatus << " / " << exitCode << ", FILE ESISTS: " << QFile::exists(m_temporaryRenderFile);
        QString error = tr("Rendering of %1 aborted when adding subtitles.").arg(m_dest);
        m_errorMessage.append(error);
        sendFinish(-2, m_errorMessage);
        QStringList args;
        if (m_frame > 0) {
            error += QLatin1Char('\n') + tr("Frame: %1").arg(m_frame);
        }
        args << QStringLiteral("--error") << error;
        m_logstream << error << "\n";
        QProcess::startDetached(QStringLiteral("kdialog"), args);
    } else {
        QFile::remove(m_dest);
        QFile::rename(m_temporaryRenderFile, m_dest);
        sendFinish(-1, QString());
    }
    QFile::remove(m_subtitleFile);
    emit renderingFinished();
    m_looper.quit();
}
