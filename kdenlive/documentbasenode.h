/***************************************************************************
                          clipmanager.h  -  Manages clips, makes sure that
			  		    they are
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

#ifndef DOCUMENTBASENODE_H
#define DOCUMENTBASENODE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qptrlist.h>

#include <kurl.h>

class DocumentClipNode;
class DocumentGroupNode;
class QDomDocument;
class KCommand;
class KdenliveDoc;

class DocumentBaseNode {
  public:
	/** Constructor for the fileclass of the application */
    DocumentBaseNode(DocumentBaseNode * parent);

	/** Destructor for the fileclass of the application */
    virtual ~ DocumentBaseNode();

    bool hasParent() const {
	return m_parent != 0;
    } 

    DocumentBaseNode *parent() {
	return m_parent;
    }

    void reParent(DocumentBaseNode * node);

	/** Returns the name of this group or clip */
    virtual const QString & name() const = 0;

	/** Returns an XML representation of this node and all of it's children. */
    QDomDocument toXML() const;

	/** Removes a child from this node. */

    DocumentBaseNode *findClipNode(const QString & name);

    void addChild(const DocumentBaseNode * node);
    void removeChild(const DocumentBaseNode * node);

    const QPtrList < DocumentBaseNode > &children() {
	return m_children;
    }
    bool hasChildren() const {
	return !m_children.isEmpty();
    } 
    virtual DocumentClipNode *asClipNode() {
	return 0;
    }
    virtual DocumentGroupNode *asGroupNode() {
	return 0;
    }
  private:
    // pointer to the parent node.
    DocumentBaseNode * m_parent;

    QPtrList < DocumentBaseNode > m_children;
};

#endif				// DOCUMENTBASENODE_H
