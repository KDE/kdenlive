/***************************************************************************
                          docclipbaseiterator.cpp  -  description
                             -------------------
    begin                : Sat Aug 10 2002
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

#include "docclipbaselist.h"

#include "krender.h"
#include "docclipavfile.h"

#include <kdebug.h>

DocClipBaseList::DocClipBaseList() :
							QPtrList<DocClipBase>()
{
	m_masterClip = 0;
}

DocClipBaseList::DocClipBaseList(const DocClipBaseList &list) :
							QPtrList<DocClipBase>(list)
{
	m_masterClip = list.masterClip();
}

DocClipBaseList &DocClipBaseList::operator=(const DocClipBaseList &list)
{
	QPtrList<DocClipBase>::operator=(list);	
	m_masterClip = list.masterClip();
	return *this;
}

DocClipBaseList::~DocClipBaseList()
{
}

QDomDocument DocClipBaseList::toXML(const QString &element)
{
	QDomDocument doc;

	QPtrListIterator<DocClipBase> itt(*this);

	doc.appendChild(doc.createElement("element"));

	while(itt.current() != 0) {
		QDomDocument clipDoc = itt.current()->toXML();
		if(m_masterClip == itt.current()) {
			clipDoc.documentElement().setAttribute("master", "true");
		}
		doc.documentElement().appendChild(doc.importNode(clipDoc.documentElement(), true));
		++itt;
	}

	return doc;
}

DocClipBase * DocClipBaseList::masterClip() const
{
	return m_masterClip;
}

void DocClipBaseList::setMasterClip(DocClipBase *clip)
{
	if(find(clip) != -1) {
		m_masterClip = clip;
	} else {
		m_masterClip = 0;
	}
}

void DocClipBaseList::generateFromXML(ClipManager &clipManager, KRender *render, QDomElement elem)
{
	if(elem.tagName() != "ClipList") {
		kdWarning() << "ClipList cannot be generated - wrong tag : " << elem.tagName() << endl;
	} else {
		QDomNode n = elem.firstChild();

		while(!n.isNull()) {
			QDomElement e = n.toElement();
			if(!e.isNull()) {
				if(e.tagName() == "clip") {
					DocClipBase *clip = DocClipBase::createClip(clipManager, e);

					if(clip) {
						append(clip);
						if(clip->isDocClipAVFile()) {
							render->getFileProperties(clip->toDocClipAVFile()->fileURL());
						}
					}
				} else {
					kdWarning() << "Unknown tag " << e.tagName() << ", skipping..." << endl;
				}
			}
			n = n.nextSibling();
		}
	}
}

