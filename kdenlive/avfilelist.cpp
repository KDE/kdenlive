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
#include "krender.h"

#include <kdebug.h>

#include <qstring.h>

AVFileList::AVFileList() :
				QPtrList<AVFile> ()
{
}

AVFileList::~AVFileList()
{
}

/** Returns an XML representation of the list. */
QDomDocument AVFileList::toXML() const
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

/** Generates the file list from the XML stored in elem. */
void AVFileList::generateFromXML(KRender *render, QDomElement elem)
{
	if(elem.tagName() != "AVFileList") {
		kdWarning() << "AVFileList cannot be generated - wrong tag : " << elem.tagName() << endl;
		return;
	}

	QDomNode n = elem.firstChild();

	while(!n.isNull()) {
		QDomElement e = n.toElement();
		if(!e.isNull()) {
			if(e.tagName() == "AVFile") {
					KURL file(e.attribute("url", ""));
					
					AVFile *av = find(file);

					if(!av) {
						av = new AVFile(file.fileName(), file);
						append(av);
						render->getFileProperties(file);
					}
			} else {
				kdWarning() << "Unknown tag " << e.tagName() << ", skipping..." << endl;
			}
		}

		n = n.nextSibling();
	}
}

/** Returns the AVFile representing the given url, or if it doesn't exist, returns 0. */
AVFile *AVFileList::find(const KURL &file)
{
	QPtrListIterator<AVFile> itt(*this);

	AVFile *av;

	while( (av = itt.current()) != 0) {
		if(av->fileURL().path() == file.path()) return av;
		++itt;
	}

	return 0;
}
