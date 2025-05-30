/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstracttask.h"
#include <memory>
#include <unordered_map>
#include <mlt++/MltConsumer.h>

class QProcess;

class StabilizeTask : public AbstractTask
{
public:
    StabilizeTask(const ObjectId &owner, const QString &binId, const QString &destination, int in, int out,
                  const std::unordered_map<QString, QVariant> &filterParams, QObject *object);
    static void start(QObject* object, bool force = false);

private Q_SLOTS:
    void processLogInfo();

protected:
    void run() override;

private:
    QString m_binId;
    int m_inPoint;
    int m_outPoint;
    std::unordered_map<QString, QVariant> m_filterParams;
    const QString m_destination;
    QStringList m_consumerArgs;
    QString m_errorMessage;
    QString m_logDetails;
    QProcess *m_jobProcess;
};
