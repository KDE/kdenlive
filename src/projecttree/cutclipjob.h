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

#ifndef CUTCLIPJOB
#define CUTCLIPJOB

#include <QObject>
#include <QProcess>

#include "abstractclipjob.h"


class CutClipJob : public AbstractClipJob
{
    Q_OBJECT

public:
    CutClipJob(CLIPTYPE cType, const QString &id, QStringList parameters);
    virtual ~ CutClipJob();
    const QString destination() const;
    QProcess *startJob(bool *ok);
    stringMap cancelProperties();
    int processLogInfo();
    bool addClipToProject;
    const QString statusMessage();
    
private:
    QString m_dest;
    QString m_src;
    QString m_start;
    QString m_end;
    QString m_cutExtraParams;
    int m_jobDuration;   
};

#endif
