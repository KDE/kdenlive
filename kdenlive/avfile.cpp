/***************************************************************************
                          avfile.cpp  -  description
                             -------------------
    begin                : Tue Jul 30 2002
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

#include <qfileinfo.h>

#include <kdebug.h>

#include "avfile.h"

KArtsDispatcher dispatcher;
KArtsServer server;

AVFile::AVFile(const QString name, const KURL url) :
						m_player(0)
{
	if(name.isNull()) {
   	setName(url.filename());
	} else {
		setName(name);
 	}

  m_refCount = 0;
  
	m_url = url;

	calculateFileProperties();
}

AVFile::~AVFile()
{
}

/** Calculates properties for this file that will be useful for the rest of the program. */
void AVFile::calculateFileProperties()
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
				m_duration= m_player->overallTime();
				if((m_duration.seconds > 0) || (m_duration.ms > 0)) {
    			flag=true;
        	break;
				}
				sleep(1);
			}

   		if(!flag) {
       	m_duration.seconds = 0;
        m_duration.ms = 0;
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
		m_duration.seconds = 0;
		m_duration.ms = 0;
		m_filesize = -1;
	}
}

/** Read property of QString m_name. */
const QString& AVFile::name()
{
	return m_name;
}

/** Write property of QString m_name. */
void AVFile::setName( const QString& _newVal){
	m_name = _newVal;
}

const QString AVFile::fileName() 
{
	return m_url.fileName();
}

const KURL AVFile::fileURL()
{
	return m_url;
}

/** returns the size of the file */
const signed int AVFile::fileSize() {
	return m_filesize;
}

/** returns the seconds part of the duration of this file. Use in combination with durationMs() to determine the total duration of the file. */
unsigned int AVFile::durationSeconds() const
{
	return m_duration.seconds;
}

unsigned int AVFile::durationMs() const
{
	return m_duration.ms;
}

unsigned int AVFile::duration() const
{
	return (m_duration.seconds * 1000) + m_duration.ms;
}

int AVFile::addReference()
{
	m_refCount++;
	return numReferences();
}
/** Removes the reference of this clip from this avFile. If the reference did not exist, a warning will be issued to stderr. Returns the number of references to avFile. */
int AVFile::removeReference()
{
	m_refCount--;
	return numReferences();
}

/** Returns the number of clips which reference this avFile. */
int AVFile::numReferences()
{
	return m_refCount;
}
