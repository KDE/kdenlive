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

#ifndef KADDCLIPCOMMAND_H
#define KADDCLIPCOMMAND_H

#include <kcommand.h>
#include <klocale.h>
#include <qdom.h>
#include <qpixmap.h>
#include <kurl.h>

#include "gentime.h"

class DocClipProject;
class DocClipRef;
class DocClipBase;
class KdenliveDoc;
class DocumentBaseNode;

/**Adds a clip to the document
  *@author Jason Wood
  */

namespace Command {

    class KAddClipCommand:public KCommand {
      public:
	/** Return a command that wil clean the DocumentBaseNode node of all of it's children. */
	static KCommand *clearChildren(DocumentBaseNode & node,
	    KdenliveDoc & document);

	/** Returns a command that will clean document of all DocumentBaseNodes. */
	static KCommand *clearProject(KdenliveDoc & document);

	/** Construct an AddClipCommand that will delete a clip */
	 KAddClipCommand(KdenliveDoc & document,
	    const QString & name,
	    DocClipBase * clip,
	    DocumentBaseNode * parent, bool create = true);

	/** Construct a video / audio clip */
	 KAddClipCommand(KdenliveDoc & document, const QString & parent, const KURL & url,
	    bool create = true);

	/** Construct a color clip */
	 KAddClipCommand(KdenliveDoc & document, const QString & parent, const QString & color,
	    const GenTime & duration, const QString & name,
	    const QString & description, bool create = true);

         /** Construct a text clip */
         KAddClipCommand(KdenliveDoc & document, const QString & parent,
                         const GenTime & duration,
                         const QString & name, const QString & description, const QDomDocument &xml,  KURL url, QPixmap &pix, bool alphaTransparency,
                         bool create = true);


	/** Add Virtual clip */
    	KAddClipCommand(KdenliveDoc & document, const QString & parent, const QString & name,
			const KURL & url, const GenTime & start, const GenTime & end, const QString & description, bool create);
         
         /** Construct an image clip */
	 KAddClipCommand(KdenliveDoc & document, const QString & parent, const KURL & url,
            const GenTime & duration, const QString & description, bool alphaTransparency,
	    bool create = true);

         /** Construct an slideshow clip */
	 KAddClipCommand(KdenliveDoc & document, const QString & parent, const KURL & url,
	    const QString & extension, const int &ttl, bool crossfade, QString lumaFile, double lumaSoftness, uint lumaduration, const GenTime & duration, const QString & description, bool alphaTransparency,
	    bool create = true);

	~KAddClipCommand();
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
	/** If true, then executing the command will create a clip, and
	unexecuting the command will delete a clip. Otherwise, it will be the
	other way around. */
	bool m_create;
	/** An xml representation of the clip */
	QDomDocument m_xmlClip;
	bool m_isTextClip;
	bool m_isVirtualClip;
	int m_id;

      private:			// Private methods
	/** Deletes the clip */
	void deleteClip();
	/** Adds the clip */
	void addClip();

    };

}				// namespace command
#endif
