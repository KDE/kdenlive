/***************************************************************************
                          documentclipnode.h  -  holds a document clip in the clip hierarchy.
                             -------------------
    begin                : Wed Sep 17 08:36:16 GMT 2003
    copyright            : (C) 2003 by Jason Wood
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

#ifndef DOCUMENTCLIPNODE_H
#define DOCUMENTCLIPNODE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kurl.h>

#include "documentbasenode.h"

class DocClipBase;
class DocClipRef;

class DocumentClipNode:public DocumentBaseNode {
  public:
	/** Constructor for the fileclass of the application */
    DocumentClipNode(DocumentBaseNode * parent, DocClipBase * base);
	/** Destructor for the fileclass of the application */
    virtual ~ DocumentClipNode();

	/** Returns the name of this clip*/
    virtual const QString & name() const;

    virtual DocumentClipNode *asClipNode() {
	return this;
    } DocClipRef *clipRef() {
	return m_ref;
    }
  private:
    // Reference to the correct clip.
    DocClipRef * m_ref;
};

#endif				// DOCUMENTCLIPNODE_H
