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

#include <qmap.h>
#include <qstring.h>
#include <qptrlist.h>

#include <kurl.h>

#include "gentime.h"

/**Holds the details of an AV file, including size, duration, length, etc. This differs from a
 DocClipAVFile so as to be able to save on memory and speed - multiple clips of the same file will
  reference the same data. Reference counting is used so that an AVFile can delete itself when it
   is no longer referenced, though this behaviour is normally activated via the project cleaning
   itself.
  *@author Jason Wood
  */  

//class DocClipAVFile;
//  
//class AVFile {
//public: 
//	~AVFile();
//	/** Write property of QString m_name. */
//	void setName( const QString& _newVal);
//	/** Read property of QString m_name. */
//	const QString& name() const;
//	QString fileName() const;
//	/** returns the size of the file */
//	signed int fileSize() const;
//
//	/** Return a list of references to this avfile. */
//	QPtrList<DocClipAVFile> references();
//	
//private: // Private attributes
//	/** The name of this AVFile. */
//	QString m_name;
//	/** A list of all DocClipAVFiles which make use of this AVFile. This is used so that we can
//	 *  clean up if we decide to delete an AVFile. */
//	QPtrList<DocClipAVFile> m_referers;
//};
//*/
#endif
