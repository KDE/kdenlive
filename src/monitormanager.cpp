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

#include "monitormanager.h"
#include <mlt++/Mlt.h>

MonitorManager::MonitorManager(QWidget *parent)
        : QObject(parent) {


}

void MonitorManager::setTimecode(Timecode tc) {
    m_timecode = tc;
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
    if (m_activeMonitor == "clip") {
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

void MonitorManager::slotPlay() {
    if (m_activeMonitor == "clip") m_clipMonitor->slotPlay();
    else m_projectMonitor->slotPlay();
}

void MonitorManager::slotRewind() {
    if (m_activeMonitor == "clip") m_clipMonitor->slotRewind();
    else m_projectMonitor->slotRewind();
}

void MonitorManager::slotForward() {
    if (m_activeMonitor == "clip") m_clipMonitor->slotForward();
    else m_projectMonitor->slotForward();
}

void MonitorManager::slotRewindOneFrame() {
    if (m_activeMonitor == "clip") m_clipMonitor->slotRewindOneFrame();
    else m_projectMonitor->slotRewindOneFrame();
}

void MonitorManager::slotForwardOneFrame() {
    if (m_activeMonitor == "clip") m_clipMonitor->slotForwardOneFrame();
    else m_projectMonitor->slotForwardOneFrame();
}

void MonitorManager::resetProfiles(QString prof) {
    m_clipMonitor->resetProfile(prof);
    m_projectMonitor->resetProfile(prof);
}

#include "monitormanager.moc"
