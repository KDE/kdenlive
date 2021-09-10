/*
 
 * SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
 *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef ABSTRACTTASK_H
#define ABSTRACTTASK_H

#include "definitions.h"

#include <QRunnable>
#include <QAtomicInt>
#include <QMutex>
#include <QObject>

class AbstractTask : public QObject, public QRunnable
{
    Q_OBJECT
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
    QAtomicInt m_isCanceled;
    QAtomicInt m_softDelete;
    bool m_isForce;
    bool m_running;
    void run() override;
    void cleanup();

private:
    //QString cacheKey();
    JOBTYPE m_type;
    int m_priority;
    void cancelJob(bool softDelete = false);
    
signals:
    void jobCanceled();
};

#endif // ABSTRACTTASK_H
