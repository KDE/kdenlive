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

MonitorManager::MonitorManager(KdenliveApp *app) :
		m_app(app),
		m_active(0)
{
}


MonitorManager::~MonitorManager()
{
}


/** Creates a new monitor and returns it. */
KMMMonitor *MonitorManager::createMonitor(KdenliveDoc *document, QWidget *parent, const char *name)
{
	m_monitors.append(new KMMMonitor(m_app, document, parent, name));
	return m_monitors.last();
}

/** Cause the specified monitor to become active. */
void MonitorManager::activateMonitor(KMMMonitor *monitor)
{
	m_active = monitor;
}

/** Returns the active monitor, or 0 if there isn't one.*/
KMMMonitor *MonitorManager::activeMonitor()
{
	return m_active;
}

bool MonitorManager::hasActiveMonitor()
{
	return m_active != 0;
}
