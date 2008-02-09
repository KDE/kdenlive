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
    : QObject(parent)
{


}

void MonitorManager::setTimecode(Timecode tc)
{
  m_timecode = tc;
}

Timecode MonitorManager::timecode()
{
  return m_timecode;
}

void MonitorManager::initMonitors(Monitor *clipMonitor, Monitor *projectMonitor)
{
  m_clipMonitor = clipMonitor;
  m_projectMonitor = projectMonitor;
  //QTimer::singleShot(1750, this, SLOT(initClipMonitor()));
  initClipMonitor();
  //initProjectMonitor();
}

void MonitorManager::initClipMonitor()
{
  m_clipMonitor->initMonitor();
  emit connectMonitors();
  //initProjectMonitor();
  //QTimer::singleShot(1500, this, SLOT(initProjectMonitor()));
}

void MonitorManager::initProjectMonitor()
{
  //m_clipMonitor->stop();
  m_projectMonitor->initMonitor();
  // activateMonitor("project");
  emit connectMonitors();
}

void MonitorManager::activateMonitor(QString name)
{
  if (m_activeMonitor == name) return;
  if (name == "clip") {
    m_projectMonitor->stop();
    m_clipMonitor->start();
    emit raiseClipMonitor(true);
  }
  else {
    m_clipMonitor->stop();
    m_projectMonitor->start();
    m_projectMonitor->raise();
    emit raiseClipMonitor(false);
  }
  m_activeMonitor = name;
}

#include "monitormanager.moc"
