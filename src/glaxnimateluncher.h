/*
    SPDX-FileCopyrightText: 2022 Meltytech, LLC
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QPoint>
#include <QSharedMemory>
#include <mlt++/MltProducer.h>

class ProjectClip;

class GlaxnimateLuncher : public QObject
{
    Q_OBJECT

    class ParentResources
    {
    public:
        std::shared_ptr<ProjectClip> m_binClip;
        int m_frameNum = -1;
        int m_clipId = -1;
    };

public:
    static GlaxnimateLuncher &instance();
    bool checkInstalled();
    void openFile(const QString &url);
    void openClip(int clipId);

private:
    std::unique_ptr<ParentResources> m_parent;
    std::unique_ptr<QDataStream> m_stream;
    std::unique_ptr<QLocalServer> m_server;
    std::unique_ptr<QSharedMemory> m_sharedMemory;
    bool m_isProtocolValid = false;
    QLocalSocket *m_socket;

    bool copyToShared(const QImage &image);
    void reset();

private slots:
    void onConnect();
    void onReadyRead();
    void onSocketError(QLocalSocket::LocalSocketError socketError);
};
