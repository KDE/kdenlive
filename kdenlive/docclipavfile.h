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

#include "docclipbase.h"

class AVFile;

class KDenliveDoc;

/**
	* Encapsulates a video, audio, picture, title, or any other kind of file that Kdenlive can support.
	* Each type of file should be encapsulated in it's own class, which should inherit this one.
  *@author Jason Wood
  */

class DocClipAVFile : public DocClipBase {
public:
	DocClipAVFile(KdenliveDoc &doc, const QString name, const KURL url);
  DocClipAVFile::DocClipAVFile(AVFile *avFile);
	~DocClipAVFile();
	QString fileName();
	
	/** Returns the duration of the file in milliseconds */
	long duration();
	/** Returns the seconds element of the duration of the file */
	long durationSeconds();
	/** Returns the milliseconds element of the duration of the file */
	long durationMs();
	/** Returns the type of this clip */
	DocClipBase::CLIPTYPE clipType();

	QDomDocument toXML();
  /** Returns the url of the AVFile this clip contains */
  KURL fileURL();
private:	
	/** A play object factory, used for calculating information, and previewing files */
	/** Determines whether this file contains audio, video or both. */
	DocClipBase::CLIPTYPE m_clipType;
  /** Holds a pointer to an AVFile which contains details of the file this clip portrays. */
  AVFile * m_avFile;
};

#endif

