/*
 
 * Copyright (c) 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ABSTRACTTASK_H
#define ABSTRACTTASK_H

#include "definitions.h"

#include <QRunnable>
#include <QMutex>
#include <QObject>

class AbstractTask : public QRunnable
{
    friend class TaskManager;

public:
    enum JOBTYPE {
        NOJOBTYPE = 0,
        PROXYJOB = 1,
        CUTJOB = 2,
        STABILIZEJOB = 3,
        TRANSCODEJOB = 4,
        FILTERCLIPJOB = 5,
        THUMBJOB = 6,
        ANALYSECLIPJOB = 7,
        LOADJOB = 8,
        AUDIOTHUMBJOB = 9,
        SPEEDJOB = 10,
        CACHEJOB = 11
    };
    AbstractTask(const ObjectId &owner, JOBTYPE type, QObject* object);
    virtual ~AbstractTask();
    static void closeAll();
    const ObjectId ownerId() const;
    bool operator==(AbstractTask& b);

protected:
    ObjectId m_owner;
    QObject* m_object;
    int m_progress;
    bool m_successful;
    bool m_isCanceled;
    bool m_isForce;
    void run() override;
    void cleanup();

private:
    //QString cacheKey();
    JOBTYPE m_type;
    int m_priority;
    QMutex m_runMutex;
};

#endif // ABSTRACTTASK_H
