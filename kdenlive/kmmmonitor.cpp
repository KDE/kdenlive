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

KMMMonitor::KMMMonitor(KdenliveApp *app, KdenliveDoc *document, QWidget *parent, const char *name ) :
										QVBox(parent,name),
                    m_screenHolder(new QVBox(this, name)),
										m_screen(new KMMScreen(app, m_screenHolder, name)),
										m_editPanel(new KMMEditPanel(document, this, name))
{
	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), this, SIGNAL(seekPositionChanged(const GenTime &)));
	connect(m_editPanel, SIGNAL(inpointPositionChanged(const GenTime &)), this, SIGNAL(inpointPositionChanged(const GenTime &)));
	connect(m_editPanel, SIGNAL(outpointPositionChanged(const GenTime &)), this, SIGNAL(outpointPositionChanged(const GenTime &)));    

  connectScreen();
}

KMMMonitor::~KMMMonitor()
{
  if(m_editPanel) delete m_editPanel;
  if(m_screen) delete m_screen;
  if(m_screenHolder) delete m_screenHolder;
}

/** Sets the length of the clip held by
this montor. FIXME - this is a
temporary function, and will be changed in the future. */
void KMMMonitor::setClipLength(int frames)
{
	m_editPanel->setClipLength(frames);
}

/** Seek the monitor to the given time. */
void KMMMonitor::seek(const GenTime &time)
{
  m_editPanel->seek(time);
}

/** This slot is called when the screen changes position. */
void KMMMonitor::screenPositionChanged(const GenTime &time)
{
	disconnect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), m_screen, SLOT(seek(const GenTime &)));    
  m_editPanel->seek(time);
	connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), m_screen, SLOT(seek(const GenTime &)));  
}

/** ***Nasty Hack***

Swaps the screens of two monitors, reparenting and reconnecting all
relevant signals/slots. This is required so that if a render instance
uses xv (of which there is only one port), we can use it in multiple
monitors. */
void KMMMonitor::swapScreens(KMMMonitor *monitor)
{
  disconnectScreen();
  monitor->disconnectScreen();

  std::swap(monitor->m_screen, m_screen);

  monitor->m_screen->reparent(monitor->m_screenHolder, 0, QPoint(0,0), true);
  m_screen->reparent(m_screenHolder, 0, QPoint(0,0), true);

  connectScreen();
  monitor->connectScreen();
}

/** Disconnects all signals/slots from the screen */
void KMMMonitor::disconnectScreen()
{
  disconnect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), m_screen, SLOT(seek(const GenTime &)));
	disconnect(m_editPanel, SIGNAL(playSpeedChanged(double)), m_screen, SLOT(play(double)));
  disconnect(m_screen, SIGNAL(rendererConnected()), m_editPanel, SLOT(rendererConnected()));
  disconnect(m_screen, SIGNAL(rendererDisconnected()), m_editPanel, SLOT(rendererDisconnected()));
  disconnect(m_screen, SIGNAL(seekPositionChanged(const GenTime &)), this, SLOT(screenPositionChanged(const GenTime &)));  
}

/** Connects all signals/slots to the screen. */
void KMMMonitor::connectScreen()
{
  connect(m_editPanel, SIGNAL(seekPositionChanged(const GenTime &)), m_screen, SLOT(seek(const GenTime &)));
	connect(m_editPanel, SIGNAL(playSpeedChanged(double)), m_screen, SLOT(play(double)));
  connect(m_screen, SIGNAL(rendererConnected()), m_editPanel, SLOT(rendererConnected()));
  connect(m_screen, SIGNAL(rendererDisconnected()), m_editPanel, SLOT(rendererDisconnected()));
  connect(m_screen, SIGNAL(seekPositionChanged(const GenTime &)), this, SLOT(screenPositionChanged(const GenTime &)));  
}

/** Set the monitors scenelist to the one specified. */
void KMMMonitor::setSceneList(const QDomDocument &scenelist)
{
  m_screen->setSceneList(scenelist);
}

void KMMMonitor::togglePlay()
{
	if(m_screen->playSpeed() == 0.0) {
		m_screen->play(1.0);
	} else {
		m_screen->play(0.0);
	}
}

const GenTime &KMMMonitor::seekPosition() const
{
	return m_screen->seekPosition();
}

void KMMMonitor::setInpoint()
{
	m_editPanel->setInpoint();
}

void KMMMonitor::setOutpoint()
{
	m_editPanel->setOutpoint();
}
