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
#include <kdebug.h>

DocClipAVFile::DocClipAVFile(KdenliveDoc *doc, const QString &name, const KURL &url) :
						DocClipBase(doc),
						m_avfile(doc->getAVFileReference(url))
{
	setTrackEnd(trackStart() + duration());
	setName(name);
	m_avfile->addReference(this);
		
	m_clipType = AV;
}

DocClipAVFile::DocClipAVFile(KdenliveDoc *doc, AVFile *avFile) :
						DocClipBase(doc),
						m_avfile(avFile)
{
	m_avfile->addReference(this);

	setTrackEnd(trackStart() + duration());
	setName(m_avfile->name());

  m_clipType = AV;
}

DocClipAVFile::~DocClipAVFile()
{
	m_avfile->removeReference(this);
	m_avfile = 0;	
}

GenTime DocClipAVFile::duration() const
{
	return m_avfile->duration();
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
	return m_avfile->fileURL();
}

/** Creates a clip from the passed QDomElement. This only pertains to those details specific to DocClipAVFile.*/
DocClipAVFile * DocClipAVFile::createClip(KdenliveDoc *doc, const QDomElement element)
{
	if(element.tagName() == "avfile") {
		KURL url(element.attribute("url"));
		return new DocClipAVFile(doc, url.fileName(), url);
	}
	
	kdWarning() << "DocClipAVFile::createClip failed to generate clip" << endl;
	return 0;
}

/** Returns true if the clip duration is known, false otherwise. */
bool DocClipAVFile::durationKnown()
{
  return m_avfile->durationKnown();
}

// virtual
int DocClipAVFile::framesPerSecond() const
{
	return m_avfile->framesPerSecond();
}

		
// virtual 
QDomDocument DocClipAVFile::generateSceneList()
{
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";

	QDomDocument scenelist;
	scenelist.appendChild(scenelist.createElement("scenelist"));

	// generate the next scene.
	QDomElement scene = scenelist.createElement("scene");
	scene.setAttribute("duration", QString::number(duration().seconds()));

	QDomElement sceneClip;
	sceneClip = scenelist.createElement("input");
	sceneClip.setAttribute(str_file, fileURL().path());
	sceneClip.setAttribute(str_inpoint, "0.0");
	sceneClip.setAttribute(str_outpoint, QString::number(duration().seconds()));
	scene.appendChild(sceneClip);
	scenelist.documentElement().appendChild(scene);

	return scenelist;
}

// virtual 
QDomDocument DocClipAVFile::sceneToXML(const GenTime &startTime, const GenTime &endTime)
{	
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";		
	QDomDocument sceneList;
	
	QDomElement sceneClip = sceneList.createElement("input");
	sceneClip.setAttribute(str_file, fileURL().path());
	sceneClip.setAttribute(str_inpoint, QString::number((startTime - trackStart() + cropStartTime()).seconds()));
	sceneClip.setAttribute(str_outpoint, QString::number((endTime - trackStart() + cropStartTime()).seconds()));

	sceneList.appendChild(sceneClip);
	return sceneList;
}

void DocClipAVFile::populateSceneTimes(QValueVector<GenTime> &toPopulate)
{
	toPopulate.append(trackStart());
	toPopulate.append(trackEnd());
}

// virtual 
bool DocClipAVFile::containsAVFile(AVFile *file)
{
	return (m_avfile == file);
}
