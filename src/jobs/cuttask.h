/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef CUTTASK_H
#define CUTTASK_H

#include "abstracttask.h"
#include <memory>
#include <unordered_map>
#include <mlt++/MltConsumer.h>

class QProcess;

class CutTask : public AbstractTask
{
public:
    CutTask(const ObjectId &owner, const QString &destination, const QStringList encodingParams, int in ,int out, bool addToProject, QObject* object);
    static void start(const ObjectId &owner, int in , int out, QObject* object, bool force = false);
    int length;

private slots:
    void processLogInfo();

protected:
    void run() override;

private:
    GenTime m_inPoint;
    GenTime m_outPoint;
    const QString m_destination;
    QStringList m_encodingParams;
    QString m_errorMessage;
    QString m_logDetails;
    std::unique_ptr<QProcess> m_jobProcess;
    int m_jobDuration;
    bool m_addToProject;
};


#endif
