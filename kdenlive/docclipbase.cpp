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

DocClipBase::DocClipBase()
{
	setTrackStart(0);
	setCropStartTime(0);
	setCropDuration(0);
}

DocClipBase::~DocClipBase()
{
}

/** Returns where the start of this clip is on the track it resides on. */
long DocClipBase::trackStartSeconds(){
	return m_trackStart.seconds;
}

long DocClipBase::trackStartMs() {
  return m_trackStart.ms;
}

long DocClipBase::trackStart() {
  return (m_trackStart.seconds * 1000) + m_trackStart.ms;
}

void DocClipBase::setTrackStart(long seconds, long ms)
{
	m_trackStart.seconds = seconds;
	m_trackStart.ms = ms;
}

void DocClipBase::setTrackStart(long ms)
{
	m_trackStart.seconds = ms/1000;
	m_trackStart.ms = ms%1000;
}

void DocClipBase::setName(QString name)
{
	m_name = name;
}

QString DocClipBase::name() 
{
	return m_name;
}

void DocClipBase::setCropStartTime(long ms)
{
	m_cropStart.seconds = ms/1000;
	m_cropStart.ms = ms%1000;
}

long DocClipBase::cropStartTime()
{
	return (m_cropStart.seconds*1000) + m_cropStart.ms;
}

void DocClipBase::setCropDuration(long ms)
{
	m_cropDuration.seconds = ms/1000;
	m_cropDuration.ms = ms%1000;
}

long DocClipBase::cropDuration()
{
	return (m_cropDuration.seconds*1000) + m_cropDuration.ms;
}


long DocClipBase::durationSeconds() {
	return duration() / 1000;
}

long DocClipBase::durationMs() {
	return duration() % 1000;
}

QDomDocument DocClipBase::toXML() {
	QDomDocument doc;

	QDomElement clip = doc.createElement("clip");	
	clip.setAttribute("name", name());
		
	QDomElement position = doc.createElement("position");
	position.setAttribute("trackstart", QString::number(trackStart()));
	position.setAttribute("cropstart", QString::number(cropStartTime()));
	position.setAttribute("cropduration", QString::number(cropDuration()));
	clip.appendChild(position);
	
	doc.appendChild(clip); 
	
	return doc;
}

DocClipBase *DocClipBase::createClip(KdenliveDoc &doc, const QDomElement element)
{
	DocClipBase *clip = 0;
	int trackStart=0;
	int cropStart=0;
	int cropDuration=0;
	
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
				trackStart = e.attribute("trackstart", 0).toInt();
				cropStart = e.attribute("cropstart", 0).toInt();
				cropDuration = e.attribute("cropduration", 0).toInt();			
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
	}

	return clip;
}

