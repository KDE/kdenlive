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

/**
 * @class AbstractClipJob
 * @brief This is the base class for all Kdenlive clip jobs.
 *
 */

class AbstractClipJob : public QObject
{
    Q_OBJECT

public:
    enum JOBTYPE {
        NOJOBTYPE = 0,
        PROXYJOB = 1,
        CUTJOB = 2,
        MLTJOB = 3,
        TRANSCODEJOB = 4,
        FILTERCLIPJOB = 5,
        THUMBJOB = 5,
        ANALYSECLIPJOB = 6
    };
    AbstractClipJob(JOBTYPE type, ClipType cType, const QString &id, QObject *parent = nullptr);
    virtual ~ AbstractClipJob();
    ClipType clipType;
    JOBTYPE jobType;
    QString description;
    bool replaceClip;
    const QString clipId() const;
    const QString errorMessage() const;
    const QString logDetails() const;
    ClipJobStatus status();
    virtual void setStatus(ClipJobStatus status);
    virtual const QString destination() const;
    virtual void startJob();
    virtual stringMap cancelProperties();
    virtual void processLogInfo();
    virtual const QString statusMessage();
    /** @brief Returns true if only one instance of this job can be run on a clip. */
    virtual bool isExclusive();
    int addClipToProject() const;
    void setAddClipToProject(int add);

protected:
    ClipJobStatus m_jobStatus;
    QString m_clipId;
    QString m_errorMessage;
    QString m_logDetails;
    int m_addClipToProject;
    QProcess *m_jobProcess;

signals:
    void jobProgress(const QString &, int, int);
    void cancelRunningJob(const QString &, const QMap<QString, QString> &);
};

#endif

