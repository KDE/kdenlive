/***************************************************************************
                          kmmscreen.h  -  description
                             -------------------
    begin                : Mon Mar 25 2002
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

#ifndef KMMSCREEN_H
#define KMMSCREEN_H

#include <qxembed.h>

#include "gentime.h"

class KRender;
class KdenliveApp;

/**KMMScreen acts as a wrapper for the window provided by the cutter.
	It requests a video window from the cutter, and embeds it within
	itself to allow seamless operation.
  *@author Jason Wood
  */

class KMMScreen : public QXEmbed  {
   Q_OBJECT
public: 
	KMMScreen(KdenliveApp *app, QWidget *parent=0, const char *name=0);
	~KMMScreen();  	
private: // Private attributes
	KRender *m_render;
  KdenliveApp *m_app;
private slots: // Private slots
  /** The renderer is ready, so we open
a video window, etc. here. */
  void rendererReady();
public slots: // Public slots
  /** Embeds the specified window. */
  void embedWindow(WId wid);
  /** Seeks to the specified time */
  void seek(GenTime time);
  /** Set the play speed of the screen */
  void play(double speed);
signals: // Signals
  /** Emitted when a renderer connects. */
  void rendererConnected();
  /** Emitted when a renderer disconnects. */
  void rendererDisconnected();  
};

#endif
