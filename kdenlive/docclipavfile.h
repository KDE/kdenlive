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

#ifndef DOCCLIPAVFILE_H
#define DOCCLIPAVFILE_H

#include <qstring.h>

#include <kurl.h>
#include <arts/kmedia2.h>
#include <arts/kartsdispatcher.h>
#include <arts/kartsserver.h>
#include <arts/kplayobjectfactory.h>

#include <docclipbase.h>

/**
	* Encapsulates a video, audio, picture, title, or any other kind of file that Kdenlive can support.
	* Each type of file should be encapsulated in it's own class, which should inherit this one.
  *@author Jason Wood
  */

class DocClipAVFile : public DocClipBase {
public:
	DocClipAVFile(QString name, KURL url);
	~DocClipAVFile();
	QString fileName();
	void setName(QString name);
	KURL fileURL();	
	/** Calculates properties for the file, including the size of the file, the duration of the file,
	 * the file format, etc. 
	 **/
	void calculateFileProperties();
	/** returns the size of the file */
	signed int fileSize();
	/** Returns the duration of the file in milliseconds */
	long duration();
	/** Returns the seconds element of the duration of the file */
	long durationSeconds();
	/** Returns the milliseconds element of the duration of the file */
	long durationMs();
	/** Returns the type of this clip */
	DocClipBase::CLIPTYPE clipType();

	QDomDocument toXML();
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
	/** Determines whether this file contains audio, video or both. */
	DocClipBase::CLIPTYPE m_clipType;
	KPlayObject *m_player;
	KPlayObjectFactory m_factory;	
};

#endif

