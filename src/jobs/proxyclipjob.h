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

#ifndef PROXYCLIPJOB
#define PROXYCLIPJOB

#include "abstractclipjob.h"

class QProcess;

class ProxyJob : public AbstractClipJob
{
    Q_OBJECT

public:
    ProxyJob(const QString &binId);
    const QString getDescription() const override;
    bool startJob() override;
    /** @brief This is to be called after the job finished.
    By design, the job should store the result of the computation but not share it with the rest of the code. This happens when we call commitResult */
    bool commitResult(Fun &undo, Fun &redo) override;

private slots:
    void processLogInfo();

private:
    int m_jobDuration;
    bool m_isFfmpegJob;
    QProcess *m_jobProcess;
    bool m_done;
};

#endif
