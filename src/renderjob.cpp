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


#include <KDebug>
#include <KLocale>
#include <kuiserverjobtracker.h>

#include "renderjob.h"
#include "timecode.h"
#include "kdenlivesettings.h"



RenderJob::RenderJob(KUrl scenelist, KUrl dest) : KJob(), m_scenelist(scenelist), m_dest(dest), m_progress(0) {
    m_renderProcess = new KProcess;
    *m_renderProcess << "inigo" << scenelist.path() << "-consumer" << "avformat:" + m_dest.path() << "progress=1";
    connect(m_renderProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotIsOver(int, QProcess::ExitStatus)));
    connect(m_renderProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(receivedStderr()));
    connect(m_renderProcess, SIGNAL(readyReadStandardError()), this, SLOT(receivedStderr()));
    m_renderProcess->setOutputChannelMode(KProcess::OnlyStderrChannel);
    m_renderProcess->start();

}


RenderJob::~RenderJob() {
}

void RenderJob::receivedStderr() {
    QString result = QString(m_renderProcess->readAllStandardError());
    result = result.simplified();
    result = result.section(" ", -1);
    int pro = result.toInt();
    if (pro > m_progress) {
        m_progress = pro;
        update();
    }
}

void RenderJob::start() {
    registerJob(this);
}

void RenderJob::registerJob(KJob *job) {
    KIO::getJobTracker()->registerJob(job);
    emit description(this, "Rendering " + m_dest.fileName(),
                     qMakePair(QString("source"), m_scenelist.path()),
                     qMakePair(QString("destination"), m_dest.path()));
}

void RenderJob::unregisterJob(KJob *job) {
    KIO::getJobTracker()->unregisterJob(job);
}


unsigned long RenderJob::percent() const {
    return m_progress;
}

void RenderJob::update() {
    setPercent(percent());
}

void RenderJob::slotIsOver(int exitcode, QProcess::ExitStatus status) {
    emitResult();
    KIO::getJobTracker()->unregisterJob(this);
}

#include "renderjob.moc"
