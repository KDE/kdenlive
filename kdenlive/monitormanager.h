/***************************************************************************
                          h  -  description
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
#ifndef MONITORMANAGER_H
#define MONITORMANAGER_H

#include "kmmmonitor.h"

#include "qobject.h"

/**
Manages the creation and life of a GUI monitor. Importantly, it makes sure that only one monitor is active at once, and keeps track of which monitor this is.

@author Jason Wood
*/
class MonitorManager : public QObject
{
   Q_OBJECT
public:
	MonitorManager(KdenliveApp *app);
	virtual ~MonitorManager();

	/** Creates a new monitor and returns it. */
	KMMMonitor *createMonitor(KdenliveDoc *document, QWidget *parent, const char *name);

	/** Returns true if Monitor Manager has a currently active monitor. */
	bool hasActiveMonitor();

	/** Cause the specified monitor to become active. */
	void activateMonitor(KMMMonitor *monitor);

	/** Returns the active monitor, or 0 if there isn't one.*/
	KMMMonitor *activeMonitor();
private:
	/** A list of all monitors */
	QPtrList<KMMMonitor> m_monitors;

	/** The application that owns this monitor manager, and to which all monitors will
	become associated. */
	KdenliveApp *m_app;

	/** The currently active monitor */
	KMMMonitor *m_active;
public slots:
	void slotMonitorClicked(KMMMonitor *monitor);
};

#endif
