/**************************1*************************************************
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

DocClipBase::DocClipBase(KdenliveDoc *doc) :
	m_trackStart(0.0),
	m_cropStart(0.0),
	m_trackEnd(0.0)	
{
	m_doc = doc;
	m_parentTrack=0;
	m_trackNum = -1;
}

DocClipBase::~DocClipBase()
{
}

const GenTime &DocClipBase::trackStart() const {
  return m_trackStart;
}

void DocClipBase::setTrackStart(const GenTime time)
{
	m_trackStart = time;
	
	if(m_parentTrack) {
		m_parentTrack->clipMoved(this);
	}
}

void DocClipBase::setName(const QString name)
{
	m_name = name;
	if(m_parentTrack) {
		m_parentTrack->clipMoved(this);
	}		
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

void DocClipBase::setTrackEnd(const GenTime &time)
{
	m_trackEnd = time;		
	if(m_parentTrack) {
		m_parentTrack->clipMoved(this);
	}	
}

GenTime DocClipBase::cropDuration()
{
	return m_trackEnd - m_trackStart;
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
	position.setAttribute("trackend", QString::number(trackEnd().seconds(), 'f', 10));
	clip.appendChild(position);
	
	doc.appendChild(clip); 
	
	return doc;
}

DocClipBase *DocClipBase::createClip(KdenliveDoc *doc, const QDomElement element)
{
	DocClipBase *clip = 0;
	GenTime trackStart;
	GenTime cropStart;
	GenTime cropDuration;
	GenTime trackEnd;
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
				trackStart = GenTime(e.attribute("trackstart", "0").toDouble());
				cropStart = GenTime(e.attribute("cropstart", "0").toDouble());
				cropDuration = GenTime(e.attribute("cropduration", "0").toDouble());
				trackEnd = GenTime(e.attribute("trackend", "-1").toDouble());
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
		if(trackEnd.seconds() != -1) {
			clip->setTrackEnd(trackEnd);		
		} else {
			clip->setTrackEnd(trackStart + cropDuration);		
		}
		clip->setParentTrack(0, trackNum);
	}

	return clip;
}

/** Sets the parent track for this clip. */
void DocClipBase::setParentTrack(DocTrackBase *track, const int trackNum)
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
	return m_trackEnd;
}

/** Returns the parentTrack of this clip. */
DocTrackBase * DocClipBase::parentTrack()
{
	return m_parentTrack;
}

/** Move the clips so that it's trackStart coincides with the time specified. */
void DocClipBase::moveTrackStart(const GenTime &time)
{	
	m_trackEnd = m_trackEnd + time - m_trackStart;
	m_trackStart = time;	
}

DocClipBase *DocClipBase::clone()
{
	return createClip(m_doc, toXML().documentElement());
}
