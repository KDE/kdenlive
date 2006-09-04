/***************************************************************************
                          clipmanager.cpp  -  description
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

#include <qdom.h>
#include <kcommand.h>

#include "docclipbase.h"
#include "docclipref.h"
#include "kaddclipcommand.h"

DocumentBaseNode::DocumentBaseNode(DocumentBaseNode * parent):
m_parent(parent)
{
}

DocumentBaseNode::~DocumentBaseNode()
{
    QPtrListIterator < DocumentBaseNode > itt(m_children);

    while (itt.current()) {
	delete itt.current();
	++itt;
    }
}

QDomDocument DocumentBaseNode::toXML() const
{
    QDomDocument doc;
    QDomElement clip = doc.createElement("node");
    clip.setAttribute("name", name());

    QPtrListIterator < DocumentBaseNode > itt(m_children);

    while (itt.current()) {
	clip.appendChild(doc.importNode(itt.current()->toXML().
		documentElement(), true));
	++itt;
    }

    doc.appendChild(clip);

    return doc;
}

DocumentBaseNode *DocumentBaseNode::findClipNode(const QString & name)
{
    DocumentBaseNode *result = 0;

    if (this->name() == name) {
	result = this;
    } else {
	QPtrListIterator < DocumentBaseNode > itt(m_children);
	while (itt.current()) {
	    result = itt.current()->findClipNode(name);
	    if (result)
		break;
	    ++itt;
	}
    }

    return result;
}

DocumentBaseNode *DocumentBaseNode::findClipNodeById(const int & id)
{
    DocumentBaseNode *result = 0;

    if (this->getId() == id) {
	result = this;
    } else {
	QPtrListIterator < DocumentBaseNode > itt(m_children);
	while (itt.current()) {
	    result = itt.current()->findClipNodeById(id);
	    if (result)
		break;
	    ++itt;
	}
    }

    return result;
}

void DocumentBaseNode::reParent(DocumentBaseNode * node)
{
    m_parent = node;
}

void DocumentBaseNode::addChild(const DocumentBaseNode * node)
{
    m_children.append(node);
}

void DocumentBaseNode::removeChild(const DocumentBaseNode * node)
{
    m_children.remove(node);
}
