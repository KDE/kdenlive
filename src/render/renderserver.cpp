/*
    SPDX-FileCopyrightText: 2021 Vincent Pinon <vpinon@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderserver.h"
#include "core.h"
#include "mainwindow.h"
#include <KLocalizedString>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>

RenderServer::RenderServer(QObject *parent)
    : QObject(parent)
{
    qWarning() << "Starting render server";
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    QString servername = QStringLiteral("org.kde.kdenlive-%1").arg(QCoreApplication::applicationPid());
    if (m_server.listen(servername)) {
        connect(&m_server, &QLocalServer::newConnection, this, &RenderServer::jobConnected);
    } else {
        pCore->displayMessage(i18n("Can't open communication with render job %1", servername), ErrorMessage);
        qWarning() << "Render server failed to listen on " << servername;
    }
    connect(pCore->window(), &MainWindow::abortRenderJob, this, &RenderServer::abortJob);
    connect(this, &RenderServer::setRenderingProgress, pCore->window(), &MainWindow::setRenderingProgress);
    connect(this, &RenderServer::setRenderingFinished, pCore->window(), &MainWindow::setRenderingFinished);
}

RenderServer::~RenderServer() {}

void RenderServer::jobConnected()
{
    QLocalSocket *socket = m_server.nextPendingConnection();
    connect(socket, &QLocalSocket::readyRead, this, &RenderServer::jobSent);
}

void RenderServer::jobSent()
{
    QLocalSocket *socket = reinterpret_cast<QLocalSocket *>(sender());
    QTextStream text(socket);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    text.setCodec("UTF-8");
#endif
    QString block, line;
    while (text.readLineInto(&line)) {
        block.append(line);
        if (line == QLatin1String("}")) { // end of json object
            QJsonParseError error;
            const QJsonObject json = QJsonDocument::fromJson(block.toUtf8(), &error).object();
            if (error.error != QJsonParseError::NoError) {
                pCore->displayMessage(i18n("Communication error with render job"), ErrorMessage);
                qWarning() << "RenderServer recieve error: " << error.errorString() << block;
            }
            handleJson(json, socket);
            block.clear();
        }
    }
}

void RenderServer::handleJson(const QJsonObject &json, QLocalSocket *socket)
{
    if (json.contains("url")) {
        m_jobSocket[json.value("url").toString()] = socket;
    }
    if (json.contains("setRenderingProgress")) {
        const QJsonObject obj = json.value("setRenderingProgress").toObject();
        const auto url = obj.value("url").toString();
        const auto progress = obj.value("progress").toInt();
        const auto frame = obj.value("frame").toInt();
        Q_EMIT setRenderingProgress(url, progress, frame);
    }
    if (json.contains("setRenderingFinished")) {
        const QJsonObject obj = json.value("setRenderingFinished").toObject();
        const auto url = obj.value("url").toString();
        const auto status = obj.value("status").toInt();
        const auto error = obj.value("error").toString();
        Q_EMIT setRenderingFinished(url, status, error);
        m_jobSocket.remove(url);
    }
}

void RenderServer::abortJob(const QString &job)
{
    if (m_jobSocket.contains(job)) {
        m_jobSocket[job]->write("abort");
        m_jobSocket[job]->flush();
    } else {
        pCore->displayMessage(i18n("Can't open communication with render job %1", job), ErrorMessage);
    }
}
