/***************************************************************************
                          avfile.h  -  description
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

#ifndef AVFILE_H
#define AVFILE_H

#include <qptrlist.h>

#include <kurl.h>
#include <arts/kmedia2.h>
#include <arts/kartsdispatcher.h>
#include <arts/kartsserver.h>
#include <arts/kplayobjectfactory.h>

#include "gentime.h"

/**Holds the details of an AV file, including size, duration, length, etc. This differs from a
 DocClipAVFile so as to be able to save on memory and speed - multiple clips of the same file will
  reference the same data. Reference counting is used so that an AVFile can delete itself when it
   is no longer referenced, though this behaviour is normally activated via the project cleaning
   itself.
  *@author Jason Wood
  */  

  
class AVFile {
public: 
	AVFile(const QString name, const KURL url);
	~AVFile();
	/** Calculates properties for the file, including the size of the file, the duration of the file,
	 * the file format, etc.
	 **/
	void calculateFileProperties();
  /** Write property of QString m_name. */
  void setName( const QString& _newVal);
  /** Read property of QString m_name. */
  const QString& name();
  const QString fileName();
	const KURL fileURL();
	/** returns the size of the file */
	const signed int fileSize();
  int numReferences();
  /** Removes the reference of this clip from this avFile. If the reference did not exist, a warning will be issued to stderr. Returns the number of references to avFile. */
  int removeReference();
  /** Adds the given avFile to the list of references. If it was already
there, then nothing happens other than a warning to stderr.

Returns the number of references to this AVFile. */
  int addReference();
	/** returns the duration of this file */
  GenTime duration() const;
private: // Private attributes
  /** Holds the url for this AV file. */
  KURL m_url;;
  /** The size in bytes of this AVFile */
  signed int m_filesize;
  /** KPlayObject used for calculating file duration, etc. */
 	KPlayObject *m_player;
	/** The duration of this file. */
	GenTime m_duration;	  
  /** The name of this AVFile. */
  QString m_name;
  /** A list of all DocClipAVFiles which make use of this AVFile. This is used so that we can clean up if we decide to delete an AVFile. */
  int m_refCount;
};

#endif
