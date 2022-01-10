/*
    SPDX-FileCopyrightText: 2021 Vincent Pinon <vpinon@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
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
    QLocalSocket* socket = m_server.nextPendingConnection();
    connect(socket, &QLocalSocket::readyRead, this, &RenderServer::jobSent);
}

void RenderServer::jobSent() {
    QLocalSocket* socket = reinterpret_cast<QLocalSocket*>(sender());
    QTextStream text(socket);
    QString block, line;
    while(text.readLineInto(&line)) {
        block.append(line);
        if(line == QLatin1String("}")) { // end of json object
            QJsonParseError error;
            const QJsonObject json = QJsonDocument::fromJson(block.toUtf8(), &error).object();
            if (error.error != QJsonParseError::NoError) {
                pCore->displayMessage(i18n("Communication error with render job"), ErrorMessage);
                qWarning() << "RenderServer recieve error: " << error.errorString() << block;
            }
            if (json.contains("url")) {
                m_jobSocket[json["url"].toString()] = socket;
            }
            if (json.contains("setRenderingProgress")) {
                emit setRenderingProgress(
                            json["setRenderingProgress"]["url"].toString(),
                            json["setRenderingProgress"]["progress"].toInt(),
                            json["setRenderingProgress"]["frame"].toInt());
            }
            if (json.contains("setRenderingFinished")) {
                emit setRenderingFinished(
                            json["setRenderingFinished"]["url"].toString(),
                            json["setRenderingFinished"]["status"].toInt(),
                            json["setRenderingFinished"]["error"].toString());
                m_jobSocket.remove(json["setRenderingFinished"]["url"].toString());
            }
            block.clear();
        }
    }
}

void RenderServer::abortJob(const QString &job) {
    if (m_jobSocket.contains(job)) {
        m_jobSocket[job]->write("abort");
        m_jobSocket[job]->flush();
    } else {
        pCore->displayMessage(i18n("Can't open communication with render job %1", job), ErrorMessage);
    }
}
