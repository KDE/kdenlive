/***************************************************************************
                          kmmscreen.cpp  -  description
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

#include "kmmscreen.h"
#include <string.h>

KMMScreen::KMMScreen(QWidget *parent, const char *name ) : QXEmbed(parent,name)
{
	setBackgroundMode(Qt::PaletteDark);

	connect(&m_socket, SIGNAL(connected()), this, SLOT(cutterConnected()));
	connect(&m_socket, SIGNAL(readyRead()), this, SLOT(readData()));
	m_socket.connectToHost("127.0.0.1", 6100);
}

KMMScreen::~KMMScreen()
{
	QString str = "quit\n";
	m_socket.writeBlock(str.latin1(), strlen(str.latin1()));
}

/** This slot is called when a connection has been established to the cutter via m_socket. */
void KMMScreen::cutterConnected()
{
	QString str = "createVideoWindow false\n";	
	m_socket.writeBlock(str.latin1(), strlen(str.latin1()));
}

/** Data is ready to be read from the socket - read it and act upon it. */
void KMMScreen::readData()
{
	QString recv;
	int result;
	
	while(m_socket.canReadLine()) {
		recv = m_socket.readLine();

		if((result = recv.find("WinID = ", 0, FALSE)) != -1) {
			result += 8;
			recv = recv.mid(result);
			unsigned long winId = recv.toULong();
			if(winId!=0) {
				embed(winId);
			}
		}
	}
}
