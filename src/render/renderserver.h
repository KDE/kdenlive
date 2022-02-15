/*
    SPDX-FileCopyrightText: 2021 Vincent Pinon <vpinon@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef RENDERSERVER_H
#define RENDERSERVER_H

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

signals:
    void setRenderingProgress(const QString &url, int progress, int frame);
    void setRenderingFinished(const QString &url, int status, const QString &error);

public slots:
    void abortJob(const QString &job);

private slots:
    void jobConnected();
    void jobSent();

private:
    QLocalServer m_server;
    QHash<QString, QLocalSocket*> m_jobSocket;
};

#endif
