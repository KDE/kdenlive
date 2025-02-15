/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstracttask.h"
#include "definitions.h"
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

#include <QList>
#include <QObject>
#include <QProcess>
#include <QRunnable>

class ProjectClip;

class MaskTask : public AbstractTask
{
public:
    enum MaskProperty { INPUTFOLDER, OUTPUTFOLDER, POINTS, LABELS, BOX, NAME, OUTPUTFILE };
    MaskTask(const ObjectId &owner, QMap<int, QString> maskProperties, std::pair<QString, QString> scriptPath, int in, int out, QObject *object);
    ~MaskTask() override;
    static void start(const ObjectId &owner, QMap<int, QString> maskProperties, std::pair<QString, QString> scriptPath, int in = 0, int out = 0,
                      QObject *object = nullptr);

protected:
    void run() override;

private:
    QMap<int, QString> m_properties;
    std::pair<QString, QString> m_scriptPath;
    int m_in;
    int m_out;
    int m_jobDuration{0};
    std::function<void()> m_readyCallBack;
    QString m_errorMessage;
    QString m_logDetails;
    std::unique_ptr<QProcess> m_scriptJob;
    bool m_isFfmpegJob{false};
    void generateMask();

private Q_SLOTS:
    void processLogInfo();
};
