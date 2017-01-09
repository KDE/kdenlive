/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "abstractclipjob.h"
#include "kdenlivesettings.h"
#include "doc/kdenlivedoc.h"

AbstractClipJob::AbstractClipJob(JOBTYPE type, ClipType cType, const QString &id, QObject *parent) :
    QObject(parent),
    clipType(cType),
    jobType(type),
    replaceClip(false),
    m_jobStatus(NoJob),
    m_clipId(id),
    m_addClipToProject(-100),
    m_jobProcess(nullptr)
{
}

AbstractClipJob::~AbstractClipJob()
{
}

int AbstractClipJob::addClipToProject() const
{
    return m_addClipToProject;
}

void AbstractClipJob::setAddClipToProject(int add)
{
    m_addClipToProject = add;
}

void AbstractClipJob::setStatus(ClipJobStatus status)
{
    m_jobStatus = status;
}

ClipJobStatus AbstractClipJob::status()
{
    return m_jobStatus;
}

const QString AbstractClipJob::clipId() const
{
    return m_clipId;
}

const QString AbstractClipJob::errorMessage() const
{
    return m_errorMessage;
}

const QString AbstractClipJob::logDetails() const
{
    return m_logDetails;
}

void AbstractClipJob::startJob()
{
}

const QString AbstractClipJob::destination() const
{
    return QString();
}

stringMap AbstractClipJob::cancelProperties()
{
    return QMap<QString, QString>();
}

void AbstractClipJob::processLogInfo()
{
}

const QString AbstractClipJob::statusMessage()
{
    return QString();
}

bool AbstractClipJob::isExclusive()
{
    return true;
}

