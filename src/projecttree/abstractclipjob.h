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

enum JOBTYPE { NOJOBTYPE = 0, PROXYJOB = 1, CUTJOB = 2};

class AbstractClipJob : public QObject
{
    Q_OBJECT

public:
    AbstractClipJob(JOBTYPE type, CLIPTYPE cType, const QString &id, QStringList parameters);    virtual ~ AbstractClipJob();
    CLIPTYPE clipType;
    CLIPJOBSTATUS jobStatus;
    JOBTYPE jobType;
    QString m_clipId;
    QString description;
    const QString clipId() const;
    const QString errorMessage() const;
    void setStatus(CLIPJOBSTATUS status);
    virtual const QString destination() const;
    virtual QProcess *startJob(bool */*ok*/);
    virtual stringMap cancelProperties();
    virtual int processLogInfo();
    virtual const QString statusMessage();
    
protected:
    QString m_errorMessage;
    QProcess *m_jobProcess;
    
private:
    
    
signals:
    void jobProgress(int progress);

};


#endif

