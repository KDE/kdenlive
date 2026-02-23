/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderjob.h"
#include "kdenlive_renderer_debug.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QStringList>

RenderJob::RenderJob(const QString &render, const QString &scenelist, const QString &target, int pid, int in, int out, const QString &subtitleFile,
                     bool debugMode, QObject *parent)
    : QObject(parent)
    , m_scenelist(scenelist)
    , m_dest(target)
    , m_progress(0)
    , m_kdenlivesocket(new QLocalSocket(this))
    , m_erase(debugMode == false && (scenelist.startsWith(QDir::tempPath()) || scenelist.startsWith(QStringLiteral("xml:%1").arg(QDir::tempPath()))))
    , m_seconds(0)
    , m_frame(0)
    , m_framein(in)
    , m_frameout(out)
    , m_pid(pid)
    , m_dualpass(false)
    , m_subtitleFile(subtitleFile)
    , m_debugMode(debugMode)
    , m_renderProcess(&m_looper)
{
    if (target == QLatin1String("/dev/null") || target == QLatin1String("NUL")) {
        m_logfile.setFileName(QDir::temp().absoluteFilePath("render.log"));
    } else {
        m_logfile.setFileName(m_dest + QStringLiteral(".log"));
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    m_renderProcess.setProgram(render);
    m_renderProcess.setProcessEnvironment(env);
    m_renderProcess.setReadChannel(QProcess::StandardError);
    connect(&m_renderProcess, &QProcess::finished, this, &RenderJob::slotIsOver);

    // Disable VDPAU so that rendering will work even if there is a Kdenlive instance using VDPAU
    qputenv("MLT_NO_VDPAU", "1");
    if (debugMode) {
        m_args = {QStringLiteral("-loglevel"), QStringLiteral("debug"), QStringLiteral("-progress2"), QUrl::toPercentEncoding(scenelist)};
    } else {
        m_args = {QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-progress2"), QUrl::toPercentEncoding(scenelist)};
    }

    // Create a log of every render process.
    if (!m_logfile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(KDENLIVE_RENDERER_LOG) << "Unable to log to" << m_logfile.fileName();
    } else {
        m_logstream.setDevice(&m_logfile);
    }
    m_connectTimer.setSingleShot(true);
    m_connectTimer.setInterval(5000);
}

RenderJob::RenderJob(const QString &errorMessage, int pid, QObject *parent)
    : QObject(parent)
    , m_progress(0)
    , m_kdenlivesocket(new QLocalSocket(this))
    , m_pid(pid)
    , m_renderProcess(&m_looper)
{
    m_renderProcess.setReadChannel(QProcess::StandardError);
    connect(&m_renderProcess, &QProcess::finished, this, &RenderJob::slotIsOver);

    QString servername = QStringLiteral("org.kde.kdenlive-%1").arg(m_pid);
    m_kdenlivesocket->connectToServer(servername);
    if (m_kdenlivesocket->waitForConnected(1000)) {
        QJsonObject method, args;
        args["status"] = -2;
        args["error"] = errorMessage;
        method["setRenderingFinished"] = args;
        m_kdenlivesocket->write(QJsonDocument(method).toJson());
        m_kdenlivesocket->flush();
        m_looper.quit();
    } else {
    }
    qApp->quit();
}

RenderJob::~RenderJob()
{
    if (m_kdenlivesocket->state() == QLocalSocket::ConnectedState) {
        m_kdenlivesocket->disconnectFromServer();
    }
    delete m_kdenlivesocket;
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
    if (m_kdenlivesocket->state() == QLocalSocket::ConnectedState) {
        QJsonObject method, args;
        args["url"] = m_dest;
        args["status"] = status;
        args["error"] = error;
        method["setRenderingFinished"] = args;
        m_kdenlivesocket->write(QJsonDocument(method).toJson());
        m_kdenlivesocket->flush();
    } else {
        if (!QFile::exists(m_dest)) {
            qCDebug(KDENLIVE_RENDERER_LOG) << "Rendering to" << m_dest << "finished. Status:" << status << "Errors: Result file does not exist!!!";
            qCDebug(KDENLIVE_RENDERER_LOG) << "JOB OUTPUT:" << m_renderProcess.readAllStandardOutput();
        } else {
            qCDebug(KDENLIVE_RENDERER_LOG) << "Rendering to" << m_dest << "finished. Status:" << status << "Errors:" << error;
        }
    }
}

void RenderJob::slotAbort()
{
    m_renderProcess.kill();
    sendFinish(-3, QString());
    if (m_erase) {
        QFile(m_scenelist).remove();
    }
    QFile(m_dest).remove();
    m_logstream << "Job aborted by user"
                << "\n";
    m_logstream.flush();
    m_logfile.close();
    qApp->quit();
}

void RenderJob::receivedStderr()
{
    QString result = QString::fromLocal8Bit(m_renderProcess.readAllStandardError());
    if (!result.contains(QLatin1Char('\n'))) {
        m_outputData.append(result);
        return;
    }
    result.prepend(m_outputData);
    m_outputData.clear();
    result = result.simplified();
    if (!result.startsWith(QLatin1String("Current Frame"))) {
        m_errorMessage.append(result + QStringLiteral("<br>"));
        m_logstream << result << "\n";
    } else {
        bool ok;
        int progress = result.section(QLatin1Char(' '), -1).toInt(&ok);
        if (!ok) {
            return;
        }
        int frame = result.section(QLatin1Char(','), 0, 0).section(QLatin1Char(' '), -1).toInt(&ok);
        if (!ok || progress <= m_progress || progress <= 0 || progress > 100 || frame < m_frame) {
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
        m_seconds = elapsedTime;
        m_frame = frame;
        updateProgress();
    }
}

void RenderJob::updateProgress()
{
    if (m_kdenlivesocket->state() == QLocalSocket::ConnectedState) {
        QJsonObject method, args;
        args["url"] = m_dest;
        args["progress"] = m_progress;
        args["frame"] = m_frame;
        method["setRenderingProgress"] = args;
        m_kdenlivesocket->write(QJsonDocument(method).toJson());
        m_kdenlivesocket->flush();
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    }
#else
    } else if (!m_connectTimer.isActive()) {
        // Linux, try to reconnect if we have a Kdenlive instance
        qCWarning(KDENLIVE_RENDERER_LOG) << "Connection to Kdenlive instance lost, trying to reconnect";
        QStringList files = QDir::temp().entryList({QStringLiteral("org.kde.kdenlive*")}, QDir::System);
        qCDebug(KDENLIVE_RENDERER_LOG) << "Searching for possible instances, results:" << files;
        if (!files.isEmpty()) {
            qCDebug(KDENLIVE_RENDERER_LOG) << "Found possible running instance:" << files.first();
            bool ok;
            int pid = files.first().section(QLatin1Char('-'), -1).toInt(&ok);
            qCDebug(KDENLIVE_RENDERER_LOG) << "Found possible running instance PID:" << pid;
            if (ok && pid > -1) {
                m_pid = pid;
                QString servername = QStringLiteral("org.kde.kdenlive-%1").arg(m_pid);
                m_kdenlivesocket->connectToServer(servername);
            }
        }
        m_connectTimer.start();
        qCDebug(KDENLIVE_RENDERER_LOG) << "Progress:" << m_progress << "%,"
                                       << "frame" << m_frame;
    }
#endif
    m_logstream << QStringLiteral("%1\t%2\t%3\n").arg(m_seconds).arg(m_frame).arg(m_progress);
}

void RenderJob::start()
{
    m_startTime = QDateTime::currentDateTime();
    if (m_pid > -1) {
        connect(m_kdenlivesocket, &QLocalSocket::connected, this, [this]() {
            QJsonObject obj;
            obj["url"] = m_dest;
            m_kdenlivesocket->write(QJsonDocument(obj).toJson());
            m_kdenlivesocket->flush();
            QJsonObject method, args;
            args["url"] = m_dest;
            args["progress"] = 0;
            args["frame"] = 0;
            method["setRenderingProgress"] = args;
            m_kdenlivesocket->write(QJsonDocument(method).toJson());
            m_kdenlivesocket->flush();
        });
        QString servername = QStringLiteral("org.kde.kdenlive-%1").arg(m_pid);
        m_kdenlivesocket->connectToServer(servername);
        if (m_kdenlivesocket->waitForConnected(1000)) {
        } else {
            qCDebug(KDENLIVE_RENDERER_LOG) << "==== RENDER SOCKET NOT CONNECTED";
        }
        connect(m_kdenlivesocket, &QLocalSocket::readyRead, this, &RenderJob::gotMessage);
    }
    // Because of the logging, we connect to stderr in all cases.
    connect(&m_renderProcess, &QProcess::readyReadStandardError, this, &RenderJob::receivedStderr);
    m_logstream << "Started render process: " << m_renderProcess.program() << ' ' << m_args.join(QLatin1Char(' ')) << "\n";
    m_renderProcess.setArguments(m_args);
    m_renderProcess.start();
    if (m_debugMode) {
        m_logstream << "Using MLT REPOSITORY: " << qgetenv("MLT_REPOSITORY") << "\n";
        m_logstream << "Using MLT DATA: " << qgetenv("MLT_DATA") << "\n";
        m_logstream << "Using MLT APPDIR: " << qgetenv("MLT_APPDIR") << "\n";
    }
    m_logstream.flush();
    m_looper.exec();
}

void RenderJob::gotMessage()
{
    if (m_kdenlivesocket) {
        const QByteArray msg = m_kdenlivesocket->readAll();
        if (msg == "abort") {
            slotAbort();
        }
    }
}

void RenderJob::slotIsOver(int exitCode, QProcess::ExitStatus status)
{
    if (m_erase) {
        QFile(m_scenelist).remove();
    }
    if (status == QProcess::CrashExit || m_renderProcess.error() != QProcess::UnknownError || exitCode != 0) {
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
    } else {
        m_logstream << "Rendering of " << m_dest << " finished"
                    << "\n";
        m_logstream.flush();
        if (m_dualpass) {
            deleteLater();
        } else {
            int error = -1;
            QString errorMessage;
            bool fileFound = false;
            if (QFile::exists(m_dest) || m_dest == QLatin1String("/dev/null") || m_dest == QLatin1String("NUL")) {
                if (!m_debugMode) {
                    m_logfile.remove();
                }
                fileFound = true;
            } else {
                // Special case, on Linux file names with an ampersand are saved using the html entity &#38;
                QString fixedDest = m_dest;
                // Special case, image sequences have the %05d replaced with a number
                if (m_dest.contains(QLatin1String("%05d"))) {
                    fixedDest.replace(QLatin1String("%05d"), QStringLiteral("00001"));
                    if (QFile::exists(fixedDest)) {
                        if (!m_debugMode) {
                            m_logfile.remove();
                        }
                        fileFound = true;
                    }
                }
                if (!fileFound && fixedDest.contains(QLatin1Char('&'))) {
                    fixedDest.replace(QLatin1Char('&'), QStringLiteral("&#38;"));
                    if (QFile::exists(fixedDest)) {
                        if (!m_debugMode) {
                            m_logfile.remove();
                        }
                        fileFound = true;
                    }
                }
            }
            if (!fileFound) {
                // Rendering finished but missing file
                error = -2;
                errorMessage = m_errorMessage;
                errorMessage.append(QLatin1Char('\n'));
                errorMessage.append(tr("Rendering of %1 aborted, resulting video will probably be corrupted.").arg(m_dest));
            }
            if (!m_subtitleFile.isEmpty()) {
                // Embed subtitles
                QString ffmpegExe = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
                if (!ffmpegExe.isEmpty()) {
                    QFileInfo videoRender(m_dest);
                    m_temporaryRenderFile = QDir::temp().absoluteFilePath(videoRender.fileName());
                    QStringList args = {
                        "-y", "-v", "quiet", "-stats", "-i", m_dest, "-i", m_subtitleFile, "-c", "copy", "-f", "matroska", m_temporaryRenderFile};
                    qCDebug(KDENLIVE_RENDERER_LOG) << "::: JOB ARGS: " << args;
                    m_progress = 0;
                    disconnect(&m_renderProcess, &QProcess::readyReadStandardError, this, &RenderJob::receivedStderr);
                    m_subsProcess.setProcessChannelMode(QProcess::MergedChannels);
                    connect(&m_subsProcess, &QProcess::readyReadStandardOutput, this, &RenderJob::receivedSubtitleProgress);
                    m_subsProcess.start(ffmpegExe, args);
                    m_subsProcess.waitForStarted(-1);
                    m_subsProcess.waitForFinished(-1);
                    slotCheckSubtitleProcess(m_subsProcess.exitCode(), m_subsProcess.exitStatus());
                    return;
                }
            }
            sendFinish(error, errorMessage);
        }
    }
    Q_EMIT renderingFinished();
    m_looper.quit();
}

void RenderJob::receivedSubtitleProgress()
{
    QString outputData = QString::fromLocal8Bit(m_subsProcess.readAllStandardOutput()).simplified();
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
        qCCritical(KDENLIVE_RENDERER_LOG) << "Subtitle process exited unexpectedly:" << exitStatus << "/" << exitCode << "; Does file exist?"
                                          << QFile::exists(m_temporaryRenderFile);
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
    Q_EMIT renderingFinished();
    m_looper.quit();
}
