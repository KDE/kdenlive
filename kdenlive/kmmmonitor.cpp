/***************************************************************************
                          kmmmonitor.cpp  -  description
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

#include "kmmmonitor.h"

KMMMonitor::KMMMonitor(KdenliveDoc *document, QWidget *parent, const char *name ) :
										QVBox(parent,name),
										m_screen(this, name),
										m_editPanel(document, this, name)
{
	connect(&m_editPanel, SIGNAL(seekPositionChanged(GenTime)), &m_screen, SLOT(seek(GenTime)));
}

KMMMonitor::~KMMMonitor(){
}

/** Sets the length of the clip held by
this montor. FIXME - this is a
temporary function, and will be changed in the future. */
void KMMMonitor::setClipLength(int frames)
{
	m_editPanel.setClipLength(frames);
}
