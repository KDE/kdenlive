/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef TRANSCODETASK_H
#define TRANSCODETASK_H

#include "abstracttask.h"

class QProcess;

class TranscodeTask : public AbstractTask
{
public:
    TranscodeTask(const ObjectId &owner, QString params, int in, int out, bool replaceProducer, QObject* object, bool checkProfile);
    static void start(const ObjectId &owner, QString params, int in, int out, bool replaceProducer, QObject* object, bool force = false, bool checkProfile = false);

protected:
    void run() override;

private slots:
    void processLogInfo();

private:
    int m_jobDuration;
    bool m_isFfmpegJob;
    QString m_transcodeParams;
    bool m_replaceProducer;
    int m_inPoint;
    int m_outPoint;
    bool m_checkProfile;
    std::unique_ptr<QProcess> m_jobProcess;
    QString m_errorMessage;
    QString m_logDetails;
};


#endif
