/***************************************************************************
                          documentclipnode.cpp  - holds a reference to a document clip in the clip hierarchy
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

#include "documentclipnode.h"

// include files for KDE
#include <klocale.h>
#include <kdebug.h>

#include "docclipref.h"

DocumentClipNode::DocumentClipNode(DocumentBaseNode *parent, DocClipBase *clip) :
			DocumentBaseNode(parent),
			m_ref( new DocClipRef(clip) )
{
}

DocumentClipNode::~DocumentClipNode()
{
	delete m_ref;
}

// virtual 
const QString &DocumentClipNode::name() const
{
	return m_ref->name();
}
