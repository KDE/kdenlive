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


#include <QObject>
#include <QTimer>

#include <mlt++/Mlt.h>

#include "monitormanager.h"
#include "kdenlivesettings.h"

MonitorManager::MonitorManager(QWidget *parent)
        : QObject(parent) {
}

Timecode MonitorManager::timecode() {
    return m_timecode;
}

void MonitorManager::initMonitors(Monitor *clipMonitor, Monitor *projectMonitor) {
    m_clipMonitor = clipMonitor;
    m_projectMonitor = projectMonitor;
}

void MonitorManager::activateMonitor(QString name) {
    if (m_activeMonitor == name) return;
    if (name == "clip") {
        m_projectMonitor->stop();
        m_clipMonitor->start();
        emit raiseClipMonitor(true);
    } else {
        m_clipMonitor->stop();
        m_projectMonitor->start();
        m_projectMonitor->raise();
        emit raiseClipMonitor(false);
    }
    m_activeMonitor = name;
}

void MonitorManager::switchMonitors() {
    if (m_clipMonitor->isActive()) {
        m_clipMonitor->stop();
        m_projectMonitor->start();
        m_projectMonitor->raise();
        m_activeMonitor = m_projectMonitor->name();
        emit raiseClipMonitor(false);
    } else {
        m_projectMonitor->stop();
        m_clipMonitor->start();
        m_activeMonitor = m_clipMonitor->name();
        emit raiseClipMonitor(true);
    }
}

void MonitorManager::stopActiveMonitor() {
    if (m_clipMonitor->isActive()) m_clipMonitor->pause();
    else m_projectMonitor->pause();
}

void MonitorManager::slotPlay() {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotPlay();
    else m_projectMonitor->slotPlay();
}

void MonitorManager::slotPlayZone() {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotPlayZone();
    else m_projectMonitor->slotPlayZone();
}

void MonitorManager::slotLoopZone() {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotLoopZone();
    else m_projectMonitor->slotLoopZone();
}

void MonitorManager::slotRewind(double speed) {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotRewind(speed);
    else m_projectMonitor->slotRewind(speed);
}

void MonitorManager::slotForward(double speed) {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotForward(speed);
    else m_projectMonitor->slotForward(speed);
}

void MonitorManager::slotRewindOneFrame() {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotRewindOneFrame();
    else m_projectMonitor->slotRewindOneFrame();
}

void MonitorManager::slotForwardOneFrame() {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotForwardOneFrame();
    else m_projectMonitor->slotForwardOneFrame();
}

void MonitorManager::slotRewindOneSecond() {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotRewindOneFrame(m_timecode.fps());
    else m_projectMonitor->slotRewindOneFrame(m_timecode.fps());
}

void MonitorManager::slotForwardOneSecond() {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotForwardOneFrame(m_timecode.fps());
    else m_projectMonitor->slotForwardOneFrame(m_timecode.fps());
}

void MonitorManager::slotStart() {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotStart();
    else m_projectMonitor->slotStart();
}

void MonitorManager::slotEnd() {
    if (m_clipMonitor->isActive()) m_clipMonitor->slotEnd();
    else m_projectMonitor->slotEnd();
}

void MonitorManager::resetProfiles(Timecode tc) {
    m_timecode = tc;
    QTimer::singleShot(300, this, SLOT(slotResetProfiles()));
}

void MonitorManager::slotResetProfiles() {
    activateMonitor("clip");
    m_clipMonitor->resetProfile();
    activateMonitor("project");
    m_projectMonitor->resetProfile();
    //m_projectMonitor->refreshMonitor(true);
}

#include "monitormanager.moc"
