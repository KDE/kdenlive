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
#include "kdenlivedoc.h"

#include <KDebug>
#include <KLocale>


AbstractClipJob::AbstractClipJob(JOBTYPE type, CLIPTYPE cType, const QString &id, QStringList) :
        QObject(),
        clipType(cType),
        jobType(type),
        jobStatus(NOJOB),
        m_clipId(id),
        addClipToProject(false),
        replaceClip(false),
        m_jobProcess(NULL)
{
}

AbstractClipJob::~AbstractClipJob()
{
}

void AbstractClipJob::setStatus(CLIPJOBSTATUS status)
{
    jobStatus = status;
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
    return QMap <QString, QString>();
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

