/***************************************************************************
                          DocClipAVFile.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 2002
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

#include "docclipavfile.h"

#include <qfileinfo.h>
#include <iostream>

KArtsDispatcher dispatcher;	
KArtsServer server;

DocClipAVFile::DocClipAVFile(QString name, KURL url) :
						DocClipBase(),
            m_player(0),
            m_factory(server.server())
{
	setName(name);
	m_url = url;

  m_factory.setAllowStreaming(false);

	calculateFileProperties();
  m_clipType = AV;
}

DocClipAVFile::~DocClipAVFile(){
}

QString DocClipAVFile::fileName() {
	return m_name;
}

void DocClipAVFile::setName(QString name) {
	m_name = name;
}

KURL DocClipAVFile::fileURL() {
	return m_url;
}


/** Calculates properties for this file that will be useful for the rest of the program. */
void DocClipAVFile::calculateFileProperties()
{
	if(m_url.isLocalFile()) {
		QFileInfo fileInfo(m_url.directory(false, false) + m_url.filename());

 		/** Determines the size of the file */		
		m_filesize = fileInfo.size();						
		
		m_player = m_factory.createPlayObject(m_url, true);

    std::cout << m_player->mediaName();
					
    if(!m_player->isNull()) {
  		
  		/** Determines the format of the file e.g. wav, ogg, mpeg, mov */		
      m_player->play();
      while(m_player->state() != EOF) {
        	 m_time= m_player->overallTime();
           if((m_time.seconds > 0) || (m_time.ms > 0)) break;
           sleep(1);
      }
      m_player->halt();
  					
  		std::cout << "Time for file is " << m_time.seconds << "." << m_time.ms << " and custom name " << m_time.customUnit << " is " << m_time.custom << std::endl;
    }
		
    if(m_player != 0) {
      delete m_player;
    }
    m_player = 0;
    sleep(2);
	} else {
		/** If the file is not local, then no file properties are currently returned */
		m_time.seconds = 0;
		m_time.ms = 0;		
		m_filesize = -1;	
	}
}

/** returns the size of the file */
signed int DocClipAVFile::fileSize() {
	return m_filesize;
}

/** Returns the seconds element of the duration of the file */
long DocClipAVFile::durationSeconds() {
	return m_time.seconds;
}

/** Returns the milliseconds element of the duration of the file */
long DocClipAVFile::durationMs() {
	return m_time.ms;
}

long DocClipAVFile::duration() {
	return (m_time.seconds * 1000) + m_time.ms;
}

/** Returns the type of this clip */
DocClipAVFile::CLIPTYPE DocClipAVFile::clipType() {
  return m_clipType;
}
