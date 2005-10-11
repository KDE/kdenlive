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

#include "kmonitor.h"

#include "qobject.h"

class KdenliveDoc;
class DocClipBase;

namespace Gui
{

class CaptureMonitor;
class KdenliveApp;
class KMMMonitor;

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

	CaptureMonitor *createCaptureMonitor(KdenliveDoc *document, QWidget *parent, const char *name);

	/** Returns true if Monitor Manager has a currently active monitor. */
	bool hasActiveMonitor();

	/** Cause the specified monitor to become active. */
	void activateMonitor(KMonitor *monitor);

	/** Returns the active monitor, or 0 if there isn't one.*/
	KMonitor *activeMonitor();

	/** Searches through monitors and clears any that are using the specified avfile. */
	void clearClip(DocClipBase *clip);
private:
	/** A list of all monitors */
	QPtrList<KMonitor> m_monitors;

	/** The application that owns this monitor manager, and to which all monitors will
	become associated. */
	KdenliveApp *m_app;

	/** The currently active monitor */
	KMonitor *m_active;
public slots:
	void slotMonitorClicked(KMonitor *monitor);
};

} // namespace Gui
#endif
