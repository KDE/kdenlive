/*

    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle (jb@kdenlive.org)

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef SCENESPLITTASK_H
#define SCENESPLITTASK_H

#include "abstracttask.h"

class QProcess;

class SceneSplitTask : public AbstractTask
{
public:
    SceneSplitTask(const ObjectId &owner, double threshold, int markersCategory, bool addSubclips, int minDuration, QObject* object);
    static void start(QObject* object, bool force = false);

protected:
    void run() override;

private slots:
    void processLogInfo();
    void processLogErr();

private:
    int m_jobDuration;
    double m_threshold;
    int m_markersType;
    bool m_subClips;
    int m_minInterval;
    std::unique_ptr<QProcess> m_jobProcess;
    QString m_errorMessage;
    QString m_logDetails;
    QList<double> m_results;
};


#endif
