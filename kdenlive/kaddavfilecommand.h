/***************************************************************************
                          kaddclipcommand.h  -  description
                             -------------------
    begin                : Fri Dec 13 2002
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

#ifndef KADDAVFILECOMMAND_H
#define KADDAVFILECOMMAND_H

#include <kcommand.h>
#include <klocale.h>
#include <kurl.h>
#include <qdom.h>

#include "gentime.h"

class KdenliveDoc;
class DocClipBase;
class AVFile;

/**Adds a clip to the document
  *@author Jason Wood
  */
//
//namespace Command {
//
//class KAddAVFileCommand : public KCommand  {
//public:
//	/** Construct an AddClipCommand that will delete a clip */
//	KAddAVFileCommand(KdenliveDoc *doc, const KURL &url, bool create=true);
//	KAddAVFileCommand(KdenliveDoc *doc, AVFile *avfile, bool create=true);
//		
//	~KAddAVFileCommand();
//	/** Unexecute the command */
//	void unexecute();
//	/** Execute the command */
//	void execute();
//	/** Returns the name of this command */
//	QString name() const;
//private: // Private methods
//	void deleteAVFile();
//private: // Private attributes
//	/** The KdenliveDoc that this action works upon. */
//	KdenliveDoc * m_doc;
//	/** The url for the avfile being added. */
//	KURL m_url;
//	/** If true, then executing the command will create a clip, and
//	unexecuting the command will delete a clip. Otherwise, it will be the
//	other way around. */
//	bool m_create;
//};
//
//} // namespace command
#endif
