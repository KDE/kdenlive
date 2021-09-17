/*

    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef PROXYTASK_H
#define PROXYTASK_H

#include "abstracttask.h"

class QProcess;

class ProxyTask : public AbstractTask
{
public:
    ProxyTask(const ObjectId &owner, QObject* object);
    static void start(const ObjectId &owner, QObject* object, bool force = false);

protected:
    void run() override;

private slots:
    void processLogInfo();

private:
    int m_jobDuration;
    bool m_isFfmpegJob;
    std::unique_ptr<QProcess> m_jobProcess;
    QString m_errorMessage;
    QString m_logDetails;
};


#endif
