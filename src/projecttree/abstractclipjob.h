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

#ifndef ABSTRACTCLIPJOB
#define ABSTRACTCLIPJOB

#include <QObject>
#include <QProcess>

#include "definitions.h"

enum JOBTYPE { NOJOBTYPE = 0, PROXYJOB = 1, CUTJOB = 2, MLTJOB = 3};

class AbstractClipJob : public QObject
{
    Q_OBJECT

public:
    AbstractClipJob(JOBTYPE type, CLIPTYPE cType, const QString &id, QStringList parameters);    virtual ~ AbstractClipJob();
    CLIPTYPE clipType;
    JOBTYPE jobType;
    CLIPJOBSTATUS jobStatus;
    QString description;
    bool replaceClip;
    const QString clipId() const;
    const QString errorMessage() const;
    const QString logDetails() const;
    void setStatus(CLIPJOBSTATUS status);
    virtual const QString destination() const;
    virtual void startJob();
    virtual stringMap cancelProperties();
    virtual void processLogInfo();
    virtual const QString statusMessage();
    /** @brief Returns true if only one instance of this job can be run on a clip. */
    virtual bool isExclusive();
    bool addClipToProject() const;
    void setAddClipToProject(bool add);
    
protected:
    QString m_clipId;
    QString m_errorMessage;
    QString m_logDetails;
    bool m_addClipToProject;
    QProcess *m_jobProcess;
    
signals:
    void jobProgress(QString, int, int);
    void cancelRunningJob(const QString, stringMap);
};


#endif

