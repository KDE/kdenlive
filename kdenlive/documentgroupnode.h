/***************************************************************************
                          documentgroupnode.h  -  a group which can hold clips or other groups.
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

#ifndef DOCUMENTGROUPNODE_H
#define DOCUMENTGROUPNODE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

#include <qstring.h>

#include <kurl.h>

#include "documentbasenode.h"

class DocumentGroupNode : public DocumentBaseNode
{
public:
	/** Constructor for the fileclass of the application */
	DocumentGroupNode(DocumentBaseNode *parent, const QString &name);
	/** Destructor for the fileclass of the application */
	virtual ~DocumentGroupNode();

	/* Returns the name of this group. */
	virtual const QString &name() const;

	virtual DocumentGroupNode *asGroupNode() { return this; }
private:
	QString m_name;
};

#endif // DOCUMENTCLIPNODE_H
