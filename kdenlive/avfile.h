/***************************************************************************
                          avfile.h  -  description
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

#ifndef AVFILE_H
#define AVFILE_H

#include <qstring.h>
#include <arts/kmedia2.h>

#include <kurl.h>

/**
	* Encapsulates a video, audio, picture, title, or any other kind of file that Kdenlive can support.
	* Each type of file should be encapsulated in it's own class, which should inherit this one.
  *@author Jason Wood
  */

class AVFile {
public: 
	AVFile(QString name, KURL url);
	~AVFile();
	QString fileName();
	void setName(QString name);
	KURL fileUrl();	
  /** Calculates properties for the file, including the size of the file, the duration of the file,  the file format, etc. */
  void calculateFileProperties();
  /** returns the size of the file */
  signed int fileSize();
private:		
	/** The displayed name of this file */
	QString m_name;
	/** The url of the file */
	KURL m_url;		
	/** The size of this file, in bytes. A negative value indicates that this is unknown */
	signed int m_filesize;
	
	/** The duration of this file. */
	Arts::poTime m_time;	
	
	/** A play object factory, used for calculating information, and previewing files */
	// Arts::PlayObjectFactory factory;
};

#endif

