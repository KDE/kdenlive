/***************************************************************************
                          kmonitor  -  description
                             -------------------
    begin                : Sun Jun 12 2005
    copyright            : (C) 2005 by Jason Wood
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
#include "kmonitor.h"

namespace Gui {

    KMonitor::KMonitor(QWidget * parent, const char *name):QVBox(parent,
	name) {
    } KMonitor::~KMonitor() {
    }


    void KMonitor::slotSetActive() {
    }

    void KMonitor::slotSetInactive() {
    }

    void KMonitor::slotClearClip() {
    }

    void KMonitor::slotToggleSnapMarker() {
    }

}
