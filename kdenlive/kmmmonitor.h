/***************************************************************************
                          kmmmonitor.h  -  description
                             -------------------
    begin                : Sun Mar 24 2002
    copyright            : (C) 2002 by Jason Wood
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

#ifndef KMMMONITOR_H
#define KMMMONITOR_H

#include <qvbox.h>

#include "kmmscreen.h"
#include "kmmeditpanel.h"
#include "gentime.h"

class KdenliveApp;
class KdenliveDoc;

/**KMMMonitor provides a multimedia bar and the
ability to play Arts PlayObjects. It is also capable
of editing the object down, and is Drag 'n drop
aware with The Timeline, The Project List, and
external files.
  *@author Jason Wood
  */

class KMMMonitor : public QVBox  {
   Q_OBJECT
public: 
	KMMMonitor(KdenliveApp *app, KdenliveDoc *document, QWidget *parent=0, const char *name=0);
	~KMMMonitor();
private:
	KMMScreen m_screen;
	KMMEditPanel m_editPanel;	
public slots: // Public slots
  /** Sets the length of the clip held by
this montor. FIXME - this is a
temporary function, and will be changed in the future. */
  void setClipLength(int frames);
  /** Seek the monitor to the given time. */
  void seek(GenTime time);
signals: // Signals
  /** Emitted when the monitor's current position has changed. */
  void seekPositionChanged(GenTime frame);
};

#endif
