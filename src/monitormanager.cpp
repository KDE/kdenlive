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


#include "monitormanager.h"
#include "renderer.h"
#include "kdenlivesettings.h"

#include <mlt++/Mlt.h>

#include <QObject>
#include <QTimer>
#include <KDebug>


MonitorManager::MonitorManager(QWidget *parent) :
        QObject(parent),
        m_clipMonitor(NULL),
        m_projectMonitor(NULL),
        m_activeMonitor(NULL)
{
}

Timecode MonitorManager::timecode()
{
    return m_timecode;
}

void MonitorManager::initMonitors(Monitor *clipMonitor, Monitor *projectMonitor, RecMonitor *recMonitor)
{
    m_clipMonitor = clipMonitor;
    m_projectMonitor = projectMonitor;

    /* MonitorManager knows the monitors logic - do it here */
    if (KdenliveSettings::jacktransport()) {
    	slotEnableSlaveTransport();
    }

    m_monitorsList.append(clipMonitor);
    m_monitorsList.append(projectMonitor);
    if (recMonitor)
        m_monitorsList.append(recMonitor);
}

void MonitorManager::appendMonitor(AbstractMonitor *monitor)
{
    if (!m_monitorsList.contains(monitor)) m_monitorsList.append(monitor);
}

void MonitorManager::removeMonitor(AbstractMonitor *monitor)
{
    m_monitorsList.removeAll(monitor);
}

AbstractMonitor* MonitorManager::monitor(Kdenlive::MONITORID monitorName)
{
    AbstractMonitor *monitor = NULL;
    for (int i = 0; i < m_monitorsList.size(); i++) {
        if (m_monitorsList[i]->id() == monitorName) {
	    monitor = m_monitorsList[i];
	}
    }
    return monitor;
}

bool MonitorManager::activateMonitor(Kdenlive::MONITORID name, bool forceRefresh)
{
    if (m_clipMonitor == NULL || m_projectMonitor == NULL)
        return false;
    if (m_activeMonitor && m_activeMonitor->id() == name) {
	if (forceRefresh) m_activeMonitor->start();
        return false;
    }
    m_activeMonitor = NULL;
    for (int i = 0; i < m_monitorsList.count(); i++) {
        if (m_monitorsList.at(i)->id() == name) {
            m_activeMonitor = m_monitorsList.at(i);
        }
        else m_monitorsList.at(i)->stop();
    }
    if (m_activeMonitor) {
        m_activeMonitor->blockSignals(true);
        m_activeMonitor->parentWidget()->raise();
        m_activeMonitor->start();
        m_activeMonitor->blockSignals(false);
    }
    emit checkColorScopes();
    return (m_activeMonitor != NULL);
}

bool MonitorManager::isActive(Kdenlive::MONITORID id) const
{
    return m_activeMonitor ? m_activeMonitor->id() == id: false;
}

void MonitorManager::slotSwitchMonitors(bool activateClip)
{
    if (activateClip)
        activateMonitor(Kdenlive::clipMonitor);
    else
        activateMonitor(Kdenlive::projectMonitor);
}

void MonitorManager::stopActiveMonitor()
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->pause();
    else if (m_activeMonitor == m_projectMonitor) m_projectMonitor->pause();
}

void MonitorManager::slotPlay()
{
    if (m_activeMonitor) m_activeMonitor->slotPlay();
}

void MonitorManager::slotPause()
{
    stopActiveMonitor();
}

void MonitorManager::slotPlayZone()
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->slotPlayZone();
    else if (m_activeMonitor == m_projectMonitor) m_projectMonitor->slotPlayZone();
}

void MonitorManager::slotLoopZone()
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->slotLoopZone();
    else m_projectMonitor->slotLoopZone();
}

void MonitorManager::slotRewind(double speed)
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->slotRewind(speed);
    else if (m_activeMonitor == m_projectMonitor) m_projectMonitor->slotRewind(speed);
}

void MonitorManager::slotForward(double speed)
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->slotForward(speed);
    else if (m_activeMonitor == m_projectMonitor) m_projectMonitor->slotForward(speed);
}

void MonitorManager::slotRewindOneFrame()
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->slotRewindOneFrame();
    else if (m_activeMonitor == m_projectMonitor) m_projectMonitor->slotRewindOneFrame();
}

void MonitorManager::slotForwardOneFrame()
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->slotForwardOneFrame();
    else if (m_activeMonitor == m_projectMonitor) m_projectMonitor->slotForwardOneFrame();
}

void MonitorManager::slotRewindOneSecond()
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->slotRewindOneFrame(m_timecode.fps());
    else if (m_activeMonitor == m_projectMonitor) m_projectMonitor->slotRewindOneFrame(m_timecode.fps());
}

void MonitorManager::slotForwardOneSecond()
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->slotForwardOneFrame(m_timecode.fps());
    else if (m_activeMonitor == m_projectMonitor) m_projectMonitor->slotForwardOneFrame(m_timecode.fps());
}

void MonitorManager::slotStart()
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->slotStart();
    else if (m_activeMonitor == m_projectMonitor) m_projectMonitor->slotStart();
}

void MonitorManager::slotEnd()
{
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->slotEnd();
    else if (m_activeMonitor == m_projectMonitor) m_projectMonitor->slotEnd();
}

void MonitorManager::resetProfiles(Timecode tc)
{
    m_timecode = tc;
    slotResetProfiles();
    //QTimer::singleShot(300, this, SLOT(slotResetProfiles()));
}

void MonitorManager::slotResetProfiles()
{
    if (m_projectMonitor == NULL || m_clipMonitor == NULL) return;
    blockSignals(true);
    Kdenlive::MONITORID active = m_activeMonitor ? m_activeMonitor->id() : Kdenlive::noMonitor;
    m_clipMonitor->resetProfile(KdenliveSettings::current_profile());
    m_projectMonitor->resetProfile(KdenliveSettings::current_profile());
    if (active != Kdenlive::noMonitor) activateMonitor(active);
    blockSignals(false);
    if (m_activeMonitor) m_activeMonitor->parentWidget()->raise();
    emit checkColorScopes();
}

void MonitorManager::slotRefreshCurrentMonitor(const QString &id)
{
    // Clip producer was modified, check if clip is currently displayed in clip monitor
    m_clipMonitor->reloadProducer(id);
    if (m_activeMonitor == m_clipMonitor) m_clipMonitor->refreshMonitor();
    else m_projectMonitor->refreshMonitor();
}

void MonitorManager::slotUpdateAudioMonitoring()
{
    // if(...) added since they are 0x0 when the config wizard is running! --Granjow
    /*if (m_clipMonitor) {
        m_clipMonitor->render->analyseAudio = KdenliveSettings::monitor_audio();
    }
    if (m_projectMonitor) {
        m_projectMonitor->render->analyseAudio = KdenliveSettings::monitor_audio();
    }*/
    for (int i = 0; i < m_monitorsList.count(); i++) {
        if (m_monitorsList.at(i)->abstractRender()) m_monitorsList.at(i)->abstractRender()->analyseAudio = KdenliveSettings::monitor_audio();
    }
}

void MonitorManager::clearScopeSource()
{
    emit clearScopes();
}

void MonitorManager::updateScopeSource()
{
    emit checkColorScopes();
}

AbstractRender *MonitorManager::activeRenderer()
{
    if (m_activeMonitor) {
        return m_activeMonitor->abstractRender();
    }
    return NULL;
}

void MonitorManager::slotSwitchFullscreen()
{
    if (m_activeMonitor) m_activeMonitor->slotSwitchFullScreen();
}

void MonitorManager::slotEnableSlaveTransport()
{
	m_projectMonitor->render->enableSlaveTransport();
}

void MonitorManager::slotDisableSlaveTransport()
{
	m_projectMonitor->render->disableSlaveTransport();
}

#include "monitormanager.moc"
