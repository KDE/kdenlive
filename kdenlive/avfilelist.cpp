/***************************************************************************
                          avfilelist.cpp  -  description
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

#include "avfilelist.h"

#include <qstring.h>

AVFileList::AVFileList(){
}
AVFileList::~AVFileList(){
}

/** Returns an XML representation of the list. */
QDomDocument AVFileList::toXML()
{
	QDomDocument doc;

	doc.appendChild(doc.createElement("AVFileList"));

	QPtrListIterator<AVFile> itt(*this);

	while(itt.current()) {
		QDomElement elem = doc.createElement("AVFile");
		elem.setAttribute("url", itt.current()->fileURL().path());
		doc.documentElement().appendChild(elem);
		++itt;
	}
	
	return doc;
}
