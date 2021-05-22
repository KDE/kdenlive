/*
 * Copyright (c) 2013-2021 Meltytech, LLC
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
