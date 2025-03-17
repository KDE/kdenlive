/*
    SPDX-FileCopyrightText: 2021 Vincent Pinon <vpinon@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QProcess>

class RenderServer : public QObject
{
    Q_OBJECT
public:
    RenderServer(QObject *parent);
    ~RenderServer() override;

Q_SIGNALS:
    void setRenderingProgress(const QString &url, int progress, int frame);
    void setRenderingFinished(const QString &url, int status, const QString &error);

public Q_SLOTS:
    void abortJob(const QString &job);

private Q_SLOTS:
    void jobConnected();
    void handleJson(const QJsonObject &json, QLocalSocket *socket);
    void jobSent();
    void abortAllJobs();

private:
    QLocalServer m_server;
    QHash<QString, QLocalSocket*> m_jobSocket;
};
