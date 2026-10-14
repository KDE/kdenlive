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
    connect(pCore->window(), &MainWindow::abortRenderJob, this, &RenderServer::abortJob, Qt::QueuedConnection);
    connect(pCore->window(), &MainWindow::abortAllRenderJobs, this, &RenderServer::abortAllJobs, Qt::DirectConnection);
    connect(this, &RenderServer::setRenderingProgress, pCore->window(), &MainWindow::setRenderingProgress);
    connect(this, &RenderServer::setRenderingFinished, pCore->window(), &MainWindow::setRenderingFinished);
}

RenderServer::~RenderServer() {}

void RenderServer::jobConnected()
{
    QLocalSocket *socket = m_server.nextPendingConnection();
    connect(socket, &QLocalSocket::readyRead, this, &RenderServer::jobSent);
    connect(socket, &QLocalSocket::disconnected, this, [this, socket]() {
        // Handle what the job sent before it exited
        readMessages(socket);
        m_buffers.remove(socket);
        for (auto it = m_jobSocket.begin(); it != m_jobSocket.end();) {
            if (it.value() == socket) {
                it = m_jobSocket.erase(it);
            } else {
                ++it;
            }
        }
        socket->deleteLater();
    });
}

QList<QByteArray> RenderServer::takeMessages(QByteArray &buffer)
{
    // The jobs send indented JSON objects (QJsonDocument::toJson()), so a message ends with a line containing only "}"
    QList<QByteArray> messages;
    const QByteArray end("\n}\n");
    int ix = buffer.indexOf(end);
    while (ix >= 0) {
        messages << buffer.left(ix + end.size());
        buffer.remove(0, ix + end.size());
        ix = buffer.indexOf(end);
    }
    return messages;
}

void RenderServer::jobSent()
{
    readMessages(qobject_cast<QLocalSocket *>(sender()));
}

void RenderServer::readMessages(QLocalSocket *socket)
{
    if (!socket || socket->bytesAvailable() <= 0) {
        return;
    }
    // A message can be split over several readyRead() signals, e.g. the job's log sent when it is finished
    QByteArray &buffer = m_buffers[socket];
    buffer.append(socket->readAll());
    const QList<QByteArray> messages = takeMessages(buffer);
    for (const QByteArray &message : messages) {
        QJsonParseError error;
        const QJsonObject json = QJsonDocument::fromJson(message, &error).object();
        if (error.error != QJsonParseError::NoError) {
            pCore->displayMessage(i18n("Communication error with render job"), ErrorMessage);
            qWarning() << "RenderServer received error: " << error.errorString() << message;
            continue;
        }
        handleJson(json, socket);
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

void RenderServer::abortAllJobs()
{
    for (auto i = m_jobSocket.cbegin(), end = m_jobSocket.cend(); i != end; ++i) {
        i.value()->write("abort");
        i.value()->flush();
    }
}
