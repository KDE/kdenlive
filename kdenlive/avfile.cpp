/***************************************************************************
                          avfile.cpp  -  description
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

#include "avfile.h"

#include <qfileinfo.h>
#include <iostream>

//Arts::PlayObject player;

AVFile::AVFile(QString name, KURL url) /*:
						 factory(Arts::Reference("global:Arts_PlayObjectFactory"))*/
{
	setName(name);
	m_url = url;

	calculateFileProperties();
}

AVFile::~AVFile(){
}

QString AVFile::fileName() {
	return m_name;
}

void AVFile::setName(QString name) {
	m_name = name;
}

KURL AVFile::fileUrl() {
	return m_url;
}


/** Calculates properties for this file that will be useful for the rest of the program. */
void AVFile::calculateFileProperties()
{	
	if(m_url.isLocalFile()) {
		QFileInfo fileInfo(m_url.directory(false, false) + m_url.filename());		
				
		cout << "File is " << (std::string)fileInfo.filePath() << endl;
		
/*		player = factory.createPlayObject((std::string)fileInfo.filePath());*/
					
		/** Determines the size of the file */		
		m_filesize = fileInfo.size();		
		
		/** Determines the format of the file e.g. wav, ogg, mpeg, mov */		
/*	  m_time= player.overallTime();*/
		m_time.seconds = 100;
		m_time.ms = 0;
					
		cout << "Time for file is " << m_time.seconds << "." << m_time.ms << " and custom name " << m_time.customUnit << " is " << m_time.custom << endl;
				
//		m_durarion = player
		
		
		/** Determines the duration (length) of the file, measured in frames */
//		m_duration = object.
				
	} else {
		/** If the file is not local, then no file properties are currently returned */
		m_time.seconds = 0;
		m_time.ms = 0;		
		m_filesize = -1;	
	}
}

/** returns the size of the file */
signed int AVFile::fileSize() {
	return m_filesize;
}
