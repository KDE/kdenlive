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
            m_player(0)
{
	if(name.isNull()) {
   	setName(url.filename());
	} else {
		setName(name);
 	}

	m_url = url;

	calculateFileProperties();
  m_clipType = AV;
}

DocClipAVFile::~DocClipAVFile(){
}

QString DocClipAVFile::fileName() {
	return m_url.fileName();
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
		
  	KPlayObjectFactory factory(server.server());
    factory.setAllowStreaming(false);

    int test = 3;
    do {
			m_player = factory.createPlayObject(m_url, true);
   		if(m_player) break;
     	sleep(1);
      test--;
  	} while(test);
								
		if(m_player && (!m_player->object().isNull()) ) {  		
			/** Determines the format of the file e.g. wav, ogg, mpeg, mov */		
			m_player->play();
			bool flag = false;
			while(m_player->state() != Arts::posIdle) {
				m_time= m_player->overallTime();
				if((m_time.seconds > 0) || (m_time.ms > 0)) {
    			flag=true;
        	break;
				}
				sleep(1);
			}

   		if(!flag) {
       	m_time.seconds = 0;
        m_time.ms = 0;
        m_filesize = -1;
      }

			m_player->halt();
		}
			
		if(m_player != 0) {
			delete m_player;
		}
		m_player = 0;
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

QDomDocument DocClipAVFile::toXML() {
	QDomDocument doc = DocClipBase::toXML();
	QDomNode node = doc.firstChild();

	while( !node.isNull()) {
		QDomElement element = node.toElement();
		if(!element.isNull()) {
			if(element.tagName() == "clip") {
				QDomElement avfile = doc.createElement("avfile");
				avfile.setAttribute("url", fileURL().url());
				element.appendChild(avfile);
				return doc;
			}
		}
		node = node.nextSibling();
	}

	ASSERT(node.isNull());

  /* This final return should never be reached, it is here to remove compiler warning. */
	return doc;
}

