/*
 * SPDX-FileCopyrightText: 2013-2021 Meltytech LLC
 * SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "abstracttask.h"
#include "core.h"
#include "bin/projectitemmodel.h"
#include "bin/projectclip.h"
#include "audio/audioStreamInfo.h"

#include <QString>
#include <QVariantList>
#include <QImage>
#include <QList>
#include <QRgb>
#include <QThreadPool>
#include <QMutex>
#include <QTime>
#include <QFile>
#include <QElapsedTimer>

AbstractTask::AbstractTask(const ObjectId &owner, JOBTYPE type, QObject* object)
    : QRunnable()
    , m_owner(owner)
    , m_object(object)
    , m_progress(0)
    , m_isCanceled(false)
    , m_softDelete(false)
    , m_isForce(false)
    , m_running(false)
    , m_type(type)
{
    setAutoDelete(true);
    switch (type) {
        case AbstractTask::LOADJOB:
            m_priority = 10;
            break;
        case AbstractTask::TRANSCODEJOB:
        case AbstractTask::PROXYJOB:
            m_priority = 8;
            break;
        case AbstractTask::FILTERCLIPJOB:
        case AbstractTask::STABILIZEJOB:
        case AbstractTask::ANALYSECLIPJOB:
        case AbstractTask::SPEEDJOB:
            m_priority = 5;
            break;
        default:
            m_priority = 5;
            break;
    }
}

void AbstractTask::cancelJob(bool softDelete)
{
    m_isCanceled.testAndSetAcquire(0, 1);
    if (softDelete) {
        m_softDelete.testAndSetAcquire(0, 1);
    }
    qDebug()<<"====== SETTING TACK CANCELED: "<<m_isCanceled;
    emit jobCanceled();
}

const ObjectId AbstractTask::ownerId() const
{
    return m_owner;
}

AbstractTask::~AbstractTask()
{
}

bool AbstractTask::operator==(AbstractTask &b)
{
    return m_owner == b.ownerId();
}

void AbstractTask::run()
{
    qDebug()<<"============0\n\nABSTRACT TASKSTARTRING\n\n==================";
}
