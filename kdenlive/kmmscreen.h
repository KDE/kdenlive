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
#include <qsocket.h>

/**KMMScreen acts as a wrapper for the window provided by the cutter.
	It requests a video window from the cutter, and embeds it within
	itself to allow seamless operation.
  *@author Jason Wood
  */

class KMMScreen : public QXEmbed  {
   Q_OBJECT
public: 
	KMMScreen(QWidget *parent=0, const char *name=0);
	~KMMScreen();  	
private: // Private attributes
  /** A socket to the cutter which provides this screen with it's view. */
  QSocket m_socket;
public slots: // Public slots
  /** This slot is called when a connection has been established to the cutter via m_socket. */
  void cutterConnected();
  /** Data is ready to be read from the socket - read it and act upon it. */
  void readData();
};

#endif
