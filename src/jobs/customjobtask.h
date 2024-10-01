/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstracttask.h"
#include <QTemporaryFile>

class QProcess;

class CustomJobTask : public AbstractTask
{
public:
    CustomJobTask(const ObjectId &owner, const QString &jobName, const QMap<QString, QString> &jobParams, int in, int out, const QString &jobId,
                  QObject *object);
    static void start(QObject *object, const QString &jobId);

protected:
    void run() override;

private Q_SLOTS:
    void processLogInfo();

private:
    int m_jobDuration;
    bool m_isFfmpegJob;
    QMap<QString, QString> m_parameters;
    QObject *m_clipPointer;
    int m_inPoint;
    int m_outPoint;
    QString m_jobId;
    std::unique_ptr<QProcess> m_jobProcess;
    QString m_errorMessage;
    QString m_logDetails;
    QTemporaryFile m_tmpFrameFile;
};
