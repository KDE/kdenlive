/***************************************************************************
                          docclipreflist.cpp  -  description
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

#include "docclipreflist.h"

#include <kdebug.h>

DocClipRefList::DocClipRefList() : QPtrList<DocClipRef>()
{
	m_masterClip = 0;
}

DocClipRefList::DocClipRefList(const DocClipRefList &list) : QPtrList<DocClipRef>(list)
{
	m_masterClip = list.masterClip();
}

DocClipRefList &DocClipRefList::operator=(const DocClipRefList &list)
{
	QPtrList<DocClipRef>::operator=(list);
	m_masterClip = list.masterClip();
	return *this;
}

DocClipRefList::~DocClipRefList()
{
}

int DocClipRefList::compareItems (QPtrCollection::Item i1, QPtrCollection::Item i2)
{
	DocClipRef *item1 = (DocClipRef *)i1;
	DocClipRef *item2 = (DocClipRef *)i2;

	const GenTime &trackStart1 = item1->trackStart();
	const GenTime &trackStart2 = item2->trackStart();

	if(trackStart1 == trackStart2) return 0;
	return (trackStart1 > trackStart2) ? 1 : -1;
}

QDomDocument DocClipRefList::toXML(const QString &element)
{
	QDomDocument doc;

	QPtrListIterator<DocClipRef> itt(*this);

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

DocClipRef * DocClipRefList::masterClip() const
{
	return m_masterClip;
}

void DocClipRefList::setMasterClip(DocClipRef *clip)
{
	if(find(clip) != -1) {
		m_masterClip = clip;
	} else {
		m_masterClip = 0;
	}
}

void DocClipRefList::appendList(const DocClipRefList &list)
{
	QPtrListIterator<DocClipRef> itt(list);

	while(itt.current())
	{
		append(itt.current());
		++itt;
	}
}
