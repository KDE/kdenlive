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
#include <qptrlist.h>

#include <kdebug.h>

#include "doctrackbase.h"
#include "doctrackclipiterator.h"

DocClipProject::DocClipProject() :
  			DocClipBase()
{
	m_framesPerSecond = 25; // Standard PAL.
	m_tracks.setAutoDelete(true);
}

DocClipProject::~DocClipProject()
{
}

GenTime DocClipProject::duration() const
{
#warning - need to be more careful about returning duration - need to cache the result and return it.
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
const KURL &DocClipProject::fileURL() const
{
	static KURL emptyUrl;

	return emptyUrl;
}

/** Returns true if the clip duration is known, false otherwise. */
bool DocClipProject::durationKnown()
{
	return true;
}

//virtual
double DocClipProject::framesPerSecond() const
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
	connect(track, SIGNAL(signalClipSelected(DocClipRef *)), this, SIGNAL(signalClipSelected(DocClipRef *)));
	connect(track, SIGNAL(clipChanged(DocClipRef *)), this, SIGNAL(clipChanged(DocClipRef *)));
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


DocTrackBase * DocClipProject::findTrack(DocClipRef *clip)
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
	// one cannot be moved to a particular location, then none of them can be movRef	// We check for the closest position the track could possibly be moved to, and move it there instead.

	int destTrackNum;
	DocTrackBase *srcTrack, *destTrack;
	GenTime clipStartTime;
	GenTime clipEndTime;
	DocClipRef *srcClip, *destClip;

	blockTrackSignals(true);

	for(uint track=0; track<numTracks(); track++) {
		srcTrack = m_tracks.at(track);
		if(!srcTrack->hasSelectedClips()) continue;

		destTrackNum = track + trackOffset;

		if((destTrackNum < 0) || (destTrackNum >= numTracks())) return false;	// This track will be moving it's clips out of the timeline, so fail automatically.

		destTrack = m_tracks.at(destTrackNum);

		QPtrListIterator<DocClipRef> srcClipItt = srcTrack->firstClip(true);
		QPtrListIterator<DocClipRef> destClipItt = destTrack->firstClip(false);

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

	QValueVector<GenTime> unsortedTimes;
        populateSceneTimes(unsortedTimes);

	// sort times and remove duplicates.
	qHeapSort(unsortedTimes);
	QValueVector<GenTime> times;

	// Will reserve much more than we need, but should speed up the process somewhat...
	times.reserve(unsortedTimes.size());

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

void DocClipProject::generateTracksFromXML(ClipManager &clipManager, const QDomElement &e)
{
	m_tracks.generateFromXML(clipManager, this, e);

	DocTrackBaseListIterator itt(m_tracks);
	while(itt.current() != 0) {
		connectTrack(itt.current());
		++itt;
	}
}

//static
DocClipProject * DocClipProject::createClip(ClipManager &clipManager, const QDomElement element)
{
	DocClipProject *project = 0;

	if(element.tagName() == "project") {
		KURL url(element.attribute("url"));
		project = new DocClipProject();
		project->m_framesPerSecond = element.attribute("fps", "25").toInt();

		QDomNode node = element.firstChild();

		while( !node.isNull()) {
			QDomElement e = node.toElement();
			if(!e.isNull()) {
				if(e.tagName() == "DocTrackBaseList") {
					project->generateTracksFromXML(clipManager, e);
				}
			}
			node = node.nextSibling();
		}
	} else {
		kdWarning() << "DocClipProject::createClip failed to generate clip" << endl;
	}

	return project;
}

void DocClipProject::populateSceneTimes(QValueVector<GenTime> &toPopulate)
{
	GenTime time;

	for(uint count=0; count<numTracks(); count++) {
		DocTrackClipIterator itt(*(m_tracks.at(count)));

		while(itt.current()) {
			QValueVector<GenTime> newTimes;
		        itt.current()->populateSceneTimes(newTimes);
			QValueVector<GenTime>::Iterator newItt = newTimes.begin();

			while(newItt != newTimes.end()) {
				time = (*newItt);
				if((time >= GenTime(0)) && (time <= duration())) {
					toPopulate.append(time);
				}
				++newItt;

			}
			++itt;
		}
	}

	toPopulate.append(GenTime(0));
	toPopulate.append(duration());
}

// Returns an XML document that describes part of the current scene.
//virtual
QDomDocument DocClipProject::sceneToXML(const GenTime &startTime, const GenTime &endTime)
{
	QDomDocument doc;

	// For the moment, this only returns the most relevant clip on the top most track.
	for(int count=0; count<numTracks(); ++count) {
		DocTrackClipIterator itt(*(m_tracks.at(count)));

		while(itt.current()) {
			DocClipRef *clip = itt.current();

			if((clip->trackStart() <= startTime) && (clip->trackEnd() >= endTime)) {
				doc = clip->sceneToXML(startTime, endTime);
				break;
			}

			++itt;
		}
	}

	return doc;
}

bool DocClipProject::hasSelectedClips()
{
	bool result = false;

	for(uint count=0; count<numTracks(); ++count) {
		if(m_tracks.at(count)->hasSelectedClips()) {
			result = true;
			break;
		}
	}

	return result;
}

DocClipRef *DocClipProject::selectedClip()
{
	DocClipRef *pResult=0;
	DocTrackBase *srcTrack=0;

	for(uint track=0; track<numTracks(); track++) {
		srcTrack = m_tracks.at(track);
		if(srcTrack->hasSelectedClips()) {
			pResult = srcTrack->firstClip(true).current();
		}
	}

	return pResult;
}

// virtual
bool DocClipProject::referencesClip(DocClipBase *clip) const
{
	bool result = false;

	DocTrackBase *srcTrack;
	uint track = 0;
	QPtrListIterator<DocTrackBase> itt(m_tracks);

	while(itt.current()) {
		srcTrack = itt.current();

		if(srcTrack->referencesClip(clip)) {
			result = true;
			break;
		}

		++itt;
		++track;
	}

	return result;
}

DocClipRefList DocClipProject::referencedClips(DocClipBase *clip)
{
	DocClipRefList list;


	DocTrackBase *srcTrack;
	for(uint track=0; track<numTracks(); ++track) {
		srcTrack = m_tracks.at(track);

		list.appendList(srcTrack->referencedClips(clip));
	}

	return list;
}

// virtual
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

// virtual
bool DocClipProject::matchesXML(const QDomElement &element)
{
	bool result = false;

	if(element.tagName() == "clip") {
		QDomNodeList nodeList = element.elementsByTagName("project");
		if(nodeList.length() > 0) {
			if(nodeList.length() > 1) {
				kdWarning() << "More than one element named project in XML,only matching XML to first one!" << endl;
			}

			QDomElement clip = nodeList.item(0).toElement();
			if(!clip.isNull()) {
# warning "this line might be the cause of significant trouble"
				if(element.attribute("fps").toDouble() == m_framesPerSecond) {
					QDomNodeList trackNodeList = clip.elementsByTagName("DocTrackBaseList");
					if(trackNodeList.length() > 0) {
						if(trackNodeList.length() > 1) {
							kdWarning() << "More than one element named 'DocTrackBaseList' in XML, matching only the first one!" << endl;
						}
						QDomElement trackElement = trackNodeList.item(0).toElement();
						if(!trackElement.isNull()) {
							result = m_tracks.matchesXML(trackElement);
						}
					}
				}
			}
		}
	}

	return result;
}
