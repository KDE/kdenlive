/***************************************************************************
                          cpp  -  description
                             -------------------
    begin                : Fri Apr 25 2003
    copyright            : (C) 2003 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "monitormanager.h"

#include "avfile.h"
#include "docclipref.h"
#include <kdebug.h>

#include "kmmmonitor.h"
#include "capturemonitor.h"

namespace Gui {

    MonitorManager::MonitorManager(KdenliveApp * app):QObject(),
	m_app(app), m_active(0) {
    } 

    MonitorManager::~MonitorManager() {
    }


/** Creates a new monitor and returns it. */
    KMMMonitor *MonitorManager::createMonitor(KdenliveDoc * document,
	QWidget * parent, const char *name) {
	KMMMonitor *monitor = new KMMMonitor(m_app, document, parent, name);
	m_monitors.append(monitor);
	connect(monitor, SIGNAL(monitorClicked(KMonitor *)), this,
	    SLOT(slotMonitorClicked(KMonitor *)));
	return monitor;
    }

    CaptureMonitor *MonitorManager::createCaptureMonitor(KdenliveDoc *
	document, QWidget * parent, const char *name) {
	CaptureMonitor *monitor = new CaptureMonitor(m_app, parent, name);
	m_monitors.append(monitor);
	connect(monitor, SIGNAL(monitorClicked(KMonitor *)), this,
	    SLOT(slotMonitorClicked(KMonitor *)));
	return monitor;
    }

/** Cause the specified monitor to become active. */
    void MonitorManager::activateMonitor(KMonitor * monitor) {
	if (m_active) {
	    if (monitor == m_active) return;
	    m_active->slotSetInactive();
	}
	m_active = monitor;
	if (m_active) {
	    m_active->slotSetActive();
	}
    }

/** Returns the active monitor, or 0 if there isn't one.*/
    KMonitor *MonitorManager::activeMonitor() {
	return m_active;
    }

    bool MonitorManager::hasActiveMonitor() {
	return m_active != 0;
    }

    void MonitorManager::slotMonitorClicked(KMonitor * monitor) {
	activateMonitor(monitor);
    }

    void MonitorManager::resetMonitors() {
	    QPtrListIterator < KMonitor > itt(m_monitors);

	    while (itt.current()) {
		if (itt.current()->clip())
			itt.current()->slotClearClip();
		++itt;
	    }
    }

    void MonitorManager::deleteMonitors() {
	m_monitors.clear();
	m_active = 0;
    }

    void MonitorManager::clearClip(DocClipBase * clip) {
	if (clip) {
	    QPtrListIterator < KMonitor > itt(m_monitors);

	    while (itt.current()) {
		if (itt.current()->clip()) {
		    if (itt.current()->clip()->referencedClip() == clip) {
			itt.current()->slotClearClip();
		    }
		}
		++itt;
	    }
	}
    }

}				// namespace Gui
