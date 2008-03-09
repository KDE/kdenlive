/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef RENDERJOB_H
#define RENDERJOB_H

#include <KJob>
#include <KUrl>
#include <KProcess>
#include <kio/job.h>
#include <kio/filejob.h>

#include "gentime.h"
#include "definitions.h"
#include "docclipbase.h"

class RenderJob : public KJob {
    Q_OBJECT
public:
    RenderJob(KUrl scenelist, KUrl dest);
    ~RenderJob();

    void registerJob(KJob *);
    void unregisterJob(KJob *);

    void start();
    unsigned long percent() const;

private slots:
    void update();
    void slotIsOver(int exitcode, QProcess::ExitStatus status);
    void receivedStderr();

private:
    KUrl m_scenelist;
    KUrl m_dest;
    int m_progress;
    KProcess *m_renderProcess;
};

#endif
