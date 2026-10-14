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
    /** @brief Removes the complete JSON messages sent by render jobs from @p buffer and returns them.
     *  Incomplete data is kept in the buffer until the rest is received. */
    static QList<QByteArray> takeMessages(QByteArray &buffer);

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
    void readMessages(QLocalSocket *socket);
    QLocalServer m_server;
    QHash<QString, QLocalSocket*> m_jobSocket;
    /** @brief Data received from each job that is not a complete message yet */
    QHash<QLocalSocket *, QByteArray> m_buffers;
};
