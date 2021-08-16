/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

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
