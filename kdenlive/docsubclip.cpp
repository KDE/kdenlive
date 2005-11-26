/***************************************************************************
                          DocSubClip.cpp  -  description
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

#include "docsubclip.h"
#include "kdenlivedoc.h"

#include "avfile.h"

#include <iostream>
#include <kdebug.h>

DocSubClip::DocSubClip(KdenliveDoc *doc, DocClipBase *clip) :
						m_clip(clip)
{
	m_clipType = AV;
}

DocSubClip::~DocSubClip()
{
	m_clip = 0;
}

const GenTime &DocSubClip::duration() const
{
	return m_clip->duration();
}

/** Returns the type of this clip */
DocSubClip::CLIPTYPE DocSubClip::clipType() {
  return m_clipType;
}

QDomDocument DocSubClip::toXML() {
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

	assert(node.isNull());

	/* This final return should never be reached, it is here to remove compiler warning. */
	return doc;
}

/** Returns the url of the AVFile this clip contains */
KURL DocSubClip::fileURL()
{
	return m_clip->fileURL();
}

/** Creates a clip from the passed QDomElement. This only pertains to those details specific to DocSubClip.*/
DocSubClip * DocSubClip::createClip(KdenliveDoc *doc, const QDomElement element)
{
#warning - needs writing.
	return 0;
}

/** Returns true if the clip duration is known, false otherwise. */
bool DocSubClip::durationKnown()
{
  return m_clip->durationKnown();
}

// virtual
double DocSubClip::framesPerSecond() const
{
	return m_clip->framesPerSecond();
}


// virtual
QDomDocument DocSubClip::generateSceneList()
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
QDomDocument DocSubClip::sceneToXML(const GenTime &startTime, const GenTime &endTime)
{
#warning - need to re-add in/out points correctly.
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";
	QDomDocument sceneList;

	QDomElement sceneClip = sceneList.createElement("input");
	sceneClip.setAttribute(str_file, fileURL().path());

	sceneList.appendChild(sceneClip);
	return sceneList;
}

void DocSubClip::populateSceneTimes(QValueVector<GenTime> &toPopulate)
{
}

// virtual
bool DocSubClip::matchesXML(const QDomElement &element)
{
# warning - needs to be written
	return false;
}

// virtual
bool DocSubClip::referencesClip(DocClipBase *clip) const
{
	return m_clip->referencesClip(clip);
}
