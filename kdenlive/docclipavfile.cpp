/***************************************************************************
                          DocClipAVFile.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 2002
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

#include "docclipavfile.h"
#include "kdenlivedoc.h"

#include "avfile.h"

#include <iostream>

DocClipAVFile::DocClipAVFile(KdenliveDoc &doc, const QString name, const KURL url) :
						DocClipBase(),
						m_avFile(doc.getAVFileReference(url))
{
	setCropDuration(duration());
	
  m_clipType = AV;
}

DocClipAVFile::DocClipAVFile(AVFile *avFile) :
						DocClipBase(),
						m_avFile(avFile)
{
	m_avFile->addReference();
	
	setCropDuration(duration());

  m_clipType = AV;
}

DocClipAVFile::~DocClipAVFile(){

	m_avFile->removeReference();
	m_avFile = 0;	
}




/** Returns the seconds element of the duration of the file */
long DocClipAVFile::durationSeconds() {
	return m_avFile->durationSeconds();
}

/** Returns the milliseconds element of the duration of the file */
long DocClipAVFile::durationMs() {
	return m_avFile->durationMs();
}

long DocClipAVFile::duration() {
	return m_avFile->duration();
}

/** Returns the type of this clip */
DocClipAVFile::CLIPTYPE DocClipAVFile::clipType() {
  return m_clipType;
}

QDomDocument DocClipAVFile::toXML() {
	QDomDocument doc = DocClipBase::toXML();
	QDomNode node = doc.firstChild();

	while( !node.isNull()) {
		QDomElement element = node.toElement();
		if(!element.isNull()) {
			if(element.tagName() == "clip") {
				QDomElement avfile = doc.createElement("avfile");
				avfile.setAttribute("url", fileURL().url());
				element.appendChild(avfile);
				return doc;
			}
		}
		node = node.nextSibling();
	}

	ASSERT(node.isNull());

  /* This final return should never be reached, it is here to remove compiler warning. */
	return doc;
}

/** Returns the url of the AVFile this clip contains */
KURL DocClipAVFile::fileURL()
{
	return m_avFile->fileURL();
}
