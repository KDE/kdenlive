/*
    SPDX-FileCopyrightText: 2021 Vincent Pinon <vpinon@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
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

public slots:
    void abortJob(QString job);

private slots:
    void jobConnected();
    void jobSent();

private:
    QLocalServer m_server;
    QHash<QString, QLocalSocket*> m_jobSocket;
};

#endif
