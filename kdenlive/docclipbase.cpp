/***************************************************************************
                          DocClipBase.cpp  -  description
                             -------------------
    begin                : Fri Apr 12 2002
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

#include <kdebug.h>

#include "docclipbase.h"
#include "docclipavfile.h"
#include "docclipproject.h"
#include "doctrackbase.h"

DocClipBase::DocClipBase() :
	m_trackStart(0.0),
	m_cropStart(0.0),	
	m_cropDuration(0.0)
{
	m_parentTrack=0;
	m_trackNum = -1;
}

DocClipBase::~DocClipBase()
{
}

const GenTime &DocClipBase::trackStart() const {
  return m_trackStart;
}

void DocClipBase::setTrackStart(GenTime time)
{
	m_trackStart = time;
	if(m_parentTrack) {
		m_parentTrack->clipMoved(this);
	}
}

void DocClipBase::setName(QString name)
{
	m_name = name;
}

QString DocClipBase::name() 
{
	return m_name;
}

void DocClipBase::setCropStartTime(const GenTime &time)
{	
	m_cropStart = time;
	if(m_parentTrack) {
		m_parentTrack->clipMoved(this);
	}	
}

const GenTime &DocClipBase::cropStartTime() const
{
	return m_cropStart;	
}

void DocClipBase::setCropDuration(const GenTime &time)
{
	m_cropDuration = time;
	if(m_parentTrack) {
		m_parentTrack->clipMoved(this);
	}	
}

GenTime DocClipBase::cropDuration()
{
	return m_cropDuration;
}

QDomDocument DocClipBase::toXML() {
	QDomDocument doc;

	QDomElement clip = doc.createElement("clip");	
	clip.setAttribute("name", name());
		
	QDomElement position = doc.createElement("position");
	position.setAttribute("track", QString::number(trackNum()));
	position.setAttribute("trackstart", QString::number(trackStart().seconds(), 'f', 10));
	position.setAttribute("cropstart", QString::number(cropStartTime().seconds(), 'f', 10));
	position.setAttribute("cropduration", QString::number(cropDuration().seconds(), 'f', 10));
	clip.appendChild(position);
	
	doc.appendChild(clip); 
	
	return doc;
}

DocClipBase *DocClipBase::createClip(KdenliveDoc &doc, const QDomElement element)
{
	DocClipBase *clip = 0;
	GenTime trackStart;
	GenTime cropStart;
	GenTime cropDuration;
	int trackNum = 0;
	
	if(element.tagName() != "clip") {
		kdWarning()	<< "DocClipBase::createClip() element has unknown tagName : " << element.tagName() << endl;
		return 0;
	}	

	QDomNode n = element.firstChild();

	while(!n.isNull()) {
		QDomElement e = n.toElement();
		if(!e.isNull()) {
			if(e.tagName() == "avfile") {
				clip = DocClipAVFile::createClip(doc, e);
			} else if(e.tagName() == "position") {
				trackNum = e.attribute("track", "-1").toInt();
				trackStart = GenTime(e.attribute("trackstart", 0).toDouble());
				cropStart = GenTime(e.attribute("cropstart", 0).toDouble());
				cropDuration = GenTime(e.attribute("cropduration", 0).toDouble());
			}		
		}
		
		n = n.nextSibling();
	}

	if(clip==0) {
	  kdWarning()	<< "DocClipBase::createClip() unable to create clip" << endl;
	} else {
		// setup DocClipBase specifics of the clip.
		clip->setTrackStart(trackStart);
		clip->setCropStartTime(cropStart);
		clip->setCropDuration(cropDuration);
		clip->setParentTrack(0, trackNum);
	}

	return clip;
}

/** Sets the parent track for this clip. */
void DocClipBase::setParentTrack(DocTrackBase *track, int trackNum)
{
	m_parentTrack = track;
	m_trackNum = trackNum;
}

/** Returns the track number. This is a hint as to which track the clip is on, or should be placed on. */
int DocClipBase::trackNum()
{
	return m_trackNum;
}

/** Returns the end of the clip on the track. A convenience function, equivalent
to trackStart() + cropDuration() */
GenTime DocClipBase::trackEnd()
{
	return trackStart() + cropDuration();
}

/** Returns the parentTrack of this clip. */
DocTrackBase * DocClipBase::parentTrack()
{
	return m_parentTrack;
}
