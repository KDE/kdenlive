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
	setAutoDelete(true);
}

DocTrackBaseList::DocTrackBaseList(const DocTrackBaseList &list) :
			QPtrList<DocTrackBase> (list)
{
	setAutoDelete(true);
}

DocTrackBaseList::~DocTrackBaseList()
{
}

/** Generates the track list, based upon the XML list provided in elem. */
void DocTrackBaseList::generateFromXML(ClipManager &clipManager, DocClipProject *project, const QDomElement &elem)
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
				DocTrackBase *track = DocTrackBase::createTrack(clipManager, project, e);
				if(track == 0) {
					kdError() << "Track not created" << endl;
				} else {
					append(track);
					track->trackIndexChanged(find(track));
				}
			} else {
				kdWarning() << "Unknown tag " << e.tagName() << ", skipping..." << endl;
			}
		}

		n = n.nextSibling();
	}
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

bool DocTrackBaseList::matchesXML(const QDomElement &element) const
{
	bool result = false;
	
	if(element.tagName() == "DocTrackBaseList") {
		QDomNodeList nodeList = element.elementsByTagName("track");

		if(nodeList.length() == count()) {
			result = true;
			QPtrListIterator<DocTrackBase> itt(*this);
			uint count=0;
			
			while(itt.current()) {
				QDomElement trackElement = nodeList.item(count).toElement();
				if(!trackElement.isNull()) {
					if(!itt.current()->matchesXML(trackElement)) {
						result = false;
						break;
					}
				} else {
					result = false;
					break;
				}

				++count;
				++itt;
			}
		}
	}
	
	return result;
}
