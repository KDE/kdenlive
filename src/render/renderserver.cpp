/*
    SPDX-FileCopyrightText: 2021 Vincent Pinon <vpinon@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "renderserver.h"
#include "core.h"
#include "mainwindow.h"
#include <KI18n/KLocalizedString>
#include <QCoreApplication>
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
        pCore->displayMessage(i18n("Can't open communication with render job %1").arg(servername), ErrorMessage);
    }
}

RenderServer::~RenderServer() {}

void RenderServer::jobConnected()
{
    QLocalSocket* socket = m_server.nextPendingConnection();
    connect(socket, &QLocalSocket::readyRead, this, &RenderServer::jobSent);
}

void RenderServer::jobSent() {
    QJsonParseError error;
    QLocalSocket* socket = reinterpret_cast<QLocalSocket*>(sender());
    const QByteArray bytes = socket->readAll();
    const QJsonObject json = QJsonDocument::fromJson(bytes, &error).object();
    if (error.error != QJsonParseError::NoError) {
        pCore->displayMessage(i18n("Communication error with render job %1 %2")
                              .arg(error.errorString(), bytes), ErrorMessage);
    }
    if (json.contains("url")) {
        m_jobSocket[json["url"].toString()] = socket;
    }
    if (json.contains("setRenderingProgress")) {
        pCore->window()->setRenderingProgress(
                    json["setRenderingProgress"]["url"].toString(),
                    json["setRenderingProgress"]["progress"].toInt(),
                    json["setRenderingProgress"]["frame"].toInt());
    }
    if (json.contains("setRenderingFinished")) {
        pCore->window()->setRenderingFinished(
                    json["setRenderingFinished"]["url"].toString(),
                    json["setRenderingFinished"]["status"].toInt(),
                    json["setRenderingFinished"]["error"].toString());
        m_jobSocket.remove(json["setRenderingFinished"]["url"].toString());
    }
}

void RenderServer::abortJob(QString job) {
    if (m_jobSocket.contains(job)) {
        m_jobSocket[job]->write("abort");
        m_jobSocket[job]->flush();
    } else {
        pCore->displayMessage(i18n("Can't open communication with render job %1").arg(job), ErrorMessage);
    }
}
