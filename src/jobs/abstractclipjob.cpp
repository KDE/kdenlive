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
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"

AbstractClipJob::AbstractClipJob(JOBTYPE type, QString id, QObject *parent)
    : QObject(parent)
    , m_clipId(std::move(id))
    , m_jobType(type)
    , m_inPoint(-1)
    , m_outPoint(-1)
{
    if (m_clipId.count(QStringLiteral("/")) == 2) {
        m_inPoint = m_clipId.section(QLatin1Char('/'), 1, 1).toInt();
        m_outPoint = m_clipId.section(QLatin1Char('/'), 2).toInt();
        m_clipId = m_clipId.section(QLatin1Char('/'), 0, 0);
    }
}

AbstractClipJob::~AbstractClipJob() = default;

const QString AbstractClipJob::clipId() const
{
    return m_clipId;
}

const QString AbstractClipJob::getErrorMessage() const
{
    return m_errorMessage;
}

const QString AbstractClipJob::getLogDetails() const
{
    return m_logDetails;
}

// static
bool AbstractClipJob::execute(const std::shared_ptr<AbstractClipJob> &job)
{
    return job->startJob();
}

AbstractClipJob::JOBTYPE AbstractClipJob::jobType() const
{
    return m_jobType;
}

