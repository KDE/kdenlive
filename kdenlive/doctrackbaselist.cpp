/***************************************************************************
                          doctrackbaselist.cpp  -  description
                             -------------------
    begin                : Thu Nov 21 2002
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

#include "doctrackbaselist.h"

#include "kdebug.h"

DocTrackBaseList::DocTrackBaseList() :
									QPtrList<DocTrackBase> ()
{
}

DocTrackBaseList::~DocTrackBaseList()
{
}

/** Returns an XML representation of this DocTrackBase list. */
QDomDocument DocTrackBaseList::toXML()
{
	QDomDocument doc;

	doc.appendChild(doc.createElement("DocTrackBaseList"));

	QPtrListIterator<DocTrackBase> itt(*this);

	while(itt.current()) {
		doc.documentElement().appendChild(doc.importNode(itt.current()->toXML().documentElement(), true));
		++itt;
	}

	return doc;
}

/** Generates the track list, based upon the XML list provided in elem. */
void DocTrackBaseList::generateFromXML(KdenliveDoc *doc, QDomElement elem)
{
	if(elem.tagName() != "DocTrackBaseList") {
		kdWarning() << "DocTrackBaseList cannot be generated - wrong tag : " << elem.tagName() << endl;
		return;
	}

	QDomNode n = elem.firstChild();

	while(!n.isNull()) {
		QDomElement e = n.toElement();
		if(!e.isNull()) {
			if(e.tagName() == "track") {
				append(DocTrackBase::createTrack(doc, e));
			} else {
				kdWarning() << "Unknown tag " << e.tagName() << ", skipping..." << endl;
			}
		}

		n = n.nextSibling();
	}
}
