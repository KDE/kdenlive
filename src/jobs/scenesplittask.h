/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstracttask.h"

class QProcess;

class SceneSplitTask : public AbstractTask
{
public:
    SceneSplitTask(const ObjectId &owner, double threshold, int markersCategory, bool rangeMarkers, bool addSubclips, int minDuration, QObject *object);
    static void start(QObject* object, bool force = false);

protected:
    void run() override;

private Q_SLOTS:
    void processLogInfo();
    void processLogErr();

private:
    double m_threshold;
    int m_jobDuration;
    int m_markersType;
    bool m_rangeMarkers;
    bool m_subClips;
    int m_minInterval;
    QProcess *m_jobProcess;
    QString m_errorMessage;
    QString m_logDetails;
    QList<double> m_results;
};
