/*
    SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstracttask.h"
#include <memory>
#include <unordered_map>

class QProcess;

class MeltTask : public AbstractTask
{
    Q_OBJECT
public:
    MeltTask(const ObjectId &owner, const QString &binId, const QString &playlistName, const QStringList &jobArgs, QObject *object);
    static void start(
        const ObjectId &owner, const QString &binId, const QString &playlistName, const QStringList &jobArgs, QObject *object,
        const std::function<void()> &readyCallBack = []() {});

private Q_SLOTS:
    void processLogInfo();

protected:
    void run() override;

private:
    QString m_binId;
    QString m_playlistName;
    QStringList m_jobArgs;
    QString m_errorMessage;
    QString m_logDetails;
    std::unique_ptr<QProcess> m_jobProcess;

Q_SIGNALS:
    void taskDone();
};
