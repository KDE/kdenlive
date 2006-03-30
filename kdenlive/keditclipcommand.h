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

#ifndef KEDITCLIPCOMMAND_H
#define KEDITCLIPCOMMAND_H

#include <kcommand.h>
#include <klocale.h>
#include <qdom.h>
#include <kurl.h>

#include "gentime.h"

class DocClipProject;
class DocClipRef;
class DocClipBase;
class KdenliveDoc;
class DocumentBaseNode;
class ProjectList;

/**Adds a clip to the document
  *@author Jason Wood
  */

namespace Command {

    class KEditClipCommand:public KCommand {
      public:

	/** Edit a color clip */
	KEditClipCommand(KdenliveDoc & document, DocClipRef * clip,
	    const QString & color, const GenTime & duration,
	    const QString & name, const QString & description);

	/** Edit an image clip */
	 KEditClipCommand(KdenliveDoc & document, DocClipRef * clip,
	    const KURL & url, const QString & extension, const int &ttl,
            const GenTime & duration, const QString & description, bool alphaTransparency);
         
         /** Edit an text clip */
         KEditClipCommand(KdenliveDoc & document, DocClipRef * clip, const GenTime & duration,
                                            const QString & name,
                                            const QString & description, const QDomDocument &xml,  KURL url, const QPixmap &pix, bool alphaTransparency);
         
	/** Edit an a/v clip */
	 KEditClipCommand(KdenliveDoc & document, DocClipRef * clip,
                          const KURL & url, const QString & description);

	~KEditClipCommand();
	/** Unexecute the command */
	void unexecute();
	/** Execute the command */
	void execute();
	/** Returns the name of this command */
	QString name() const;
      private:			// Private attributes
	 KdenliveDoc & m_document;
	/** The name of the clip. */
	QString m_name;
	/** The name of it's parent. */
	QString m_parent;
	/** An xml representation of the clip */
	QDomDocument m_xmlClip;

      private:			// Private methods
	/** Adds the clip */
	void addClip();
    };

}				// namespace command
#endif
