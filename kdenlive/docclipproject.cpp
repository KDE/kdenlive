/***************************************************************************
                          docclipproject.cpp  -  description
                             -------------------
    begin                : Thu Jun 20 2002
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

#include "docclipproject.h"

#include <qptrvector.h>
#include <qvaluevector.h>

#include <kdebug.h>

#include "doctrackbase.h"
#include "doctrackclipiterator.h"

DocClipProject::DocClipProject(KdenliveDoc *doc) :
  			DocClipBase(doc)
{
	m_framesPerSecond = 25; // Standard PAL.
	m_tracks.setAutoDelete(true);
}

DocClipProject::~DocClipProject()
{
}

GenTime DocClipProject::duration() const
{
	GenTime length(0);

	QPtrListIterator<DocTrackBase> itt(m_tracks);

	while(itt.current()) {
		GenTime test = itt.current()->trackLength();
		length = (test > length) ? test : length;
		++itt;
	}


	return length;
}

/** No descriptions */
KURL DocClipProject::fileURL()
{
	KURL temp;

	return temp;
}

QDomDocument DocClipProject::toXML() 
{
	QDomDocument doc = DocClipBase::toXML();
	QDomNode node = doc.firstChild();

	while( !node.isNull()) {
		QDomElement element = node.toElement();
		if(!element.isNull()) {
			if(element.tagName() == "clip") {
				QDomElement project = doc.createElement("project");
				project.setAttribute("fps", QString::number(m_framesPerSecond));
				element.appendChild(project);
				project.appendChild(doc.importNode(m_tracks.toXML().documentElement(), true));
				
				return doc;
			}
		}
		node = node.nextSibling();
	}

	ASSERT(node.isNull());

	return doc;
}

/** Returns true if the clip duration is known, false otherwise. */
bool DocClipProject::durationKnown()
{
	return true;
}

//virtual
int DocClipProject::framesPerSecond() const
{
	return m_framesPerSecond;
}

/** Adds a track to the project */
void DocClipProject::addTrack(DocTrackBase *track){
	m_tracks.append(track);
	track->trackIndexChanged(trackIndex(track));
	connectTrack(track);
	emit trackListChanged();
}

/** Returns the index value for this track, or -1 on failure.*/
int DocClipProject::trackIndex(DocTrackBase *track)
{
	return m_tracks.find(track);
}

void DocClipProject::connectTrack(DocTrackBase *track)
{
	connect(track, SIGNAL(clipLayoutChanged()), this, SIGNAL(clipLayoutChanged()));
	connect(track, SIGNAL(signalClipSelected(DocClipBase *)), this, SIGNAL(signalClipSelected(DocClipBase *)));
}
	
uint DocClipProject::numTracks() const
{
	return m_tracks.count();
}

DocTrackBase * DocClipProject::firstTrack()
{
	return m_tracks.first();
}

DocTrackBase * DocClipProject::nextTrack()
{
	return m_tracks.next();
}


DocTrackBase * DocClipProject::findTrack(DocClipBase *clip)
{
	DocTrackBase *returnTrack = 0;
	
	QPtrListIterator<DocTrackBase> itt(m_tracks);

	for(DocTrackBase *track;(track = itt.current()); ++itt) {
		if(track->clipExists(clip)) {
			returnTrack = track;
			break;
		}
	}
	
	return returnTrack;
}

DocTrackBase * DocClipProject::track(int track)
{
	return m_tracks.at(track);
}

bool DocClipProject::moveSelectedClips(GenTime startOffset, int trackOffset)
{
	// For each track, check and make sure that the clips can be moved to their rightful place. If
	// one cannot be moved to a particular location, then none of them can be moved.
	// We check for the closest position the track could possibly be moved to, and move it there instead.
  
	int destTrackNum;
	DocTrackBase *srcTrack, *destTrack;
	GenTime clipStartTime;
	GenTime clipEndTime;
	DocClipBase *srcClip, *destClip;

	blockTrackSignals(true);

	for(uint track=0; track<numTracks(); track++) {
		srcTrack = m_tracks.at(track);
		if(!srcTrack->hasSelectedClips()) continue;

		destTrackNum = track + trackOffset;

		if((destTrackNum < 0) || (destTrackNum >= numTracks())) return false;	// This track will be moving it's clips out of the timeline, so fail automatically.

		destTrack = m_tracks.at(destTrackNum);

		QPtrListIterator<DocClipBase> srcClipItt = srcTrack->firstClip(true);
		QPtrListIterator<DocClipBase> destClipItt = destTrack->firstClip(false);

		destClip = destClipItt.current();

		while( (srcClip = srcClipItt.current()) != 0) {
			clipStartTime = srcClipItt.current()->trackStart() + startOffset;
			clipEndTime = clipStartTime + srcClipItt.current()->cropDuration();

			while((destClip) && (destClip->trackStart() + destClip->cropDuration() <= clipStartTime)) {
				++destClipItt;
				destClip = destClipItt.current();
			}
			if(destClip==0) break;

			if(destClip->trackStart() < clipEndTime) {
			        blockTrackSignals(false);
			        return false;
			}

			++srcClipItt;
		}
	}
  
	// we can now move all clips where they need to be.

	// If the offset is negative, handle tracks from forwards, else handle tracks backwards. We
	// do this so that there are no collisions between selected clips, which would be caught by DocTrackBase
	// itself.

	int startAtTrack, endAtTrack, direction;

	if(trackOffset < 0) {
		startAtTrack = 0;
		endAtTrack = numTracks();
		direction = 1;
	} else {
		startAtTrack = numTracks() - 1;
		endAtTrack = -1;
		direction = -1;
	}

	for(int track=startAtTrack; track!=endAtTrack; track += direction) {
		srcTrack = m_tracks.at(track);
		if(!srcTrack->hasSelectedClips()) continue;
		srcTrack->moveClips(startOffset, true);

		if(trackOffset) {
			destTrackNum = track + trackOffset;
			destTrack = m_tracks.at(destTrackNum);
			destTrack->addClips(srcTrack->removeClips(true), true);
		}
	}

	blockTrackSignals(false);

	return true;
}

void DocClipProject::blockTrackSignals(bool block)
{
	QPtrListIterator<DocTrackBase> itt(m_tracks);

	while(itt.current() != 0)
	{
		itt.current()->blockSignals(block);
		++itt;
	}
}

QDomDocument DocClipProject::generateSceneList()
{
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";		

	QDomDocument sceneList;
	sceneList.appendChild(sceneList.createElement("scenelist"));

	QValueVector<GenTime> unsortedTimes = sceneTimes();

	// sort times and remove duplicates.
	qHeapSort(unsortedTimes);
	QValueVector<GenTime> times;
	
	QValueVector<GenTime>::Iterator itt = unsortedTimes.begin();
	while(itt != unsortedTimes.end()) {
		if((times.isEmpty()) || ((*itt) != times.last())) {
			times.append(*itt);
		}
		++itt;
	}	
	
	QValueVector<GenTime>::Iterator sceneItt = times.begin();

	while( (sceneItt != times.end()) && (sceneItt+1 != times.end()) ) {
		QDomElement scene = sceneList.createElement("scene");
		scene.setAttribute("duration", QString::number((*(sceneItt+1) - *sceneItt).seconds()));

		QDomDocument clipDoc = sceneToXML(*sceneItt, *(sceneItt+1));
		QDomElement sceneClip;
		
		if(clipDoc.documentElement().isNull()) {
			sceneClip = sceneList.createElement("stillcolor");
			sceneClip.setAttribute("yuvcolor", "#000000");
			scene.appendChild(sceneClip);
		} else {
			sceneClip = sceneList.importNode(clipDoc.documentElement(), true).toElement();
			scene.appendChild(sceneClip);
		}
		
		sceneList.documentElement().appendChild(scene);
		
		++sceneItt;
	}

	return sceneList;
}

void DocClipProject::generateTracksFromXML(const QDomElement &e)
{
	m_tracks.generateFromXML(m_document, e);

	DocTrackBaseListIterator itt(m_tracks);
	while(itt.current() != 0) {
		connectTrack(itt.current());
		++itt;
	}
}

//static 
DocClipProject * DocClipProject::createClip(KdenliveDoc *doc, const QDomElement element)
{
	DocClipProject *project = 0;
	
	if(element.tagName() == "project") {
		KURL url(element.attribute("url"));
		project = new DocClipProject(doc);
		project->m_framesPerSecond = element.attribute("fps", "25").toInt();
		
		QDomNode node = element.firstChild();

		while( !node.isNull()) {
			QDomElement e = node.toElement();
			if(!e.isNull()) {
				if(e.tagName() == "DocTrackBaseList") {
					project->generateTracksFromXML(e);
				}
			}
			node = node.nextSibling();
		}
	} else {
		kdWarning() << "DocClipProject::createClip failed to generate clip" << endl;
	}
	
	return project;
}

QValueVector<GenTime> DocClipProject::sceneTimes()
{
	QValueVector<GenTime> list;
	GenTime time;

	for(uint count=0; count<numTracks(); count++) {
		DocTrackClipIterator itt(*(m_tracks.at(count)));

		while(itt.current()) {
			QValueVector<GenTime> newTimes = itt.current()->sceneTimes();
			QValueVector<GenTime>::Iterator newItt = newTimes.begin();
			
			while(newItt != newTimes.end()) {
				time = (*newItt) + trackStart() - cropStartTime();
				if((time >= trackStart()) && (time <= trackEnd())) {
					list.append(time);
				}
				++newItt;
				
			}
			++itt;
		}
	}
	
	list.append(trackStart());
	list.append(trackEnd());
	
	return list;
}

// Returns an XML document that describes part of the current scene.
//virtual 
QDomDocument DocClipProject::sceneToXML(const GenTime &startTime, const GenTime &endTime)
{
	QDomDocument doc;

	GenTime newStart = startTime - trackStart() + cropStartTime();
	GenTime newEnd = endTime - trackStart() + cropStartTime();

	// For the moment, this only returns the most relevant clip on the top most track.
	for(int count=numTracks()-1; count>=0; --count) {
		DocTrackClipIterator itt(*(m_tracks.at(count)));

		while(itt.current()) {
			DocClipBase *clip = itt.current();

			if((clip->trackStart() <= newStart) && (clip->trackEnd() >= newEnd)) {
				doc = clip->sceneToXML(newStart, newEnd);
				break;
			}
			
			++itt;
		}
	}

	return doc;
}
