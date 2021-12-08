/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef SPEEDTASK_H
#define SPEEDTASK_H

#include "abstracttask.h"
#include <memory>
#include <unordered_map>
#include <mlt++/MltConsumer.h>

class QProcess;

class SpeedTask : public AbstractTask
{
public:
    SpeedTask(const ObjectId &owner, const QString &binId, const QString &destination, int in, int out, std::unordered_map<QString, QVariant> filterParams, QObject* object);
    static void start(QObject* object, bool force = false);

private slots:
    void processLogInfo();

protected:
    void run() override;

private:
    QString m_binId;
    double m_speed;
    int m_inPoint;
    int m_outPoint;
    QString m_assetId;
    QString m_filterName;
    std::unordered_map<QString, QVariant> m_filterParams;
    const QString m_destination;
    QStringList m_consumerArgs;
    QString m_errorMessage;
    QString m_logDetails;
    bool m_addToFolder;
    std::unique_ptr<QProcess> m_jobProcess;
};


#endif
