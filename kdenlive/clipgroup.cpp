/***************************************************************************
                          clipgroup.cpp  -  description
                             -------------------
    begin                : Thu Aug 22 2002
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

#include "kdenlivedoc.h"
#include "clipgroup.h"
#include "docclipbase.h"
#include "doctrackbase.h"

ClipGroup::ClipGroup()
{
	m_master = 0;
}

ClipGroup::~ClipGroup()
{
}

/** Adds a clip to the clip group. If the clip is already in the group, it will not be added a second time. */
void ClipGroup::addClip(DocClipBase *clip, DocTrackBase *track)
{
	ClipGroupClip newClip;

	newClip.clip = clip;
	newClip.track = track;
	
	if(!clipExists(clip)) {
		m_clipList.append(newClip);
		setMasterClip(clip);
	}
}

/** Removes the selected clip from the clip group. If the clip passed is not in the clip group,
 nothing (other than a warning) will happen. */
void ClipGroup::removeClip(DocClipBase *clip)
{		
	QValueListIterator<ClipGroupClip> itt;
	
	if((itt = findClip(clip)) != m_clipList.end()) {
		removeClip(itt);
	}
}

/** Returns true if the specified clip is in the group. */
bool ClipGroup::clipExists(DocClipBase *clip)
{
	return findClip(clip) != m_clipList.end();
}

/** Removes the clip if it is in the list, adds it if it isn't. Returns true if the clip
is now part of the list, false if it is not. */
bool ClipGroup::toggleClip(DocClipBase *clip, DocTrackBase *track)
{
	QValueListIterator<ClipGroupClip> itt;

	itt = findClip(clip);

	if( itt != m_clipList.end()) {
		removeClip(itt);
		return false;
	}

	addClip(clip, track);	
	return true;
}

/** Moves all clips by the offset specified. Returns true if successful, fasle on failure. */
bool ClipGroup::moveByOffset(int offset)
{
	QValueListIterator<ClipGroupClip> itt;
	DocClipBase *clip;
	
	itt = m_clipList.begin();	
	while( (clip = (*itt).clip)) {
		++itt;
		
		clip->setTrackStart(GenTime(clip->trackStart().frames(25) + offset, 25));	
	}

	return true;
}

/** Removes all clips from the clip group.
 */
void ClipGroup::removeAllClips()
{
	m_clipList.clear();
	setMasterClip(NULL);
}

/** Find space on the timeline where the clips in the clipGroup will fit. Returns true if successful, initialising newValue to the given offset. If no place can be found, returns false. */
bool ClipGroup::findClosestMatchingSpace(int value, int &newValue)
{
	if(m_master==0) return false;

	newValue = value;
	
	return true;
}

/** Returns a clip list of all clips contained within the group. */
QPtrList<DocClipBase> ClipGroup::clipList()
{
	QPtrList<DocClipBase> clipList;

	clipList.setAutoDelete(false);

	QValueListIterator<ClipGroupClip> itt;

	for(itt = m_clipList.begin(); itt != m_clipList.end(); ++itt) {
		clipList.append((*itt).clip);
	}
		
	return clipList;		
}

/** returns true is this clipGroup is currently empty, false otherwise. */
bool ClipGroup::isEmpty()
{
	return m_clipList.count()==0;
}

void ClipGroup::setMasterClip(DocClipBase *master)
{
	if((master==0) || (!clipExists(master))) {
		if(m_clipList.isEmpty()) {
			m_master = 0;
		} else {
			m_master = m_clipList.first().clip;
		}
	} else {
		m_master = master;
	}
}

/** Moves all clips from their current location to a new location. The location points specified
are those where the current Master Clip should end up. All other clips move relative to this clip.
Only the master clip necessarily needs to reside on the specified track. Clip priority will never
change - Display order will be preserved.*/
void ClipGroup::moveTo(KdenliveDoc &doc, int track, int value)
{
	DocTrackBase *curTrack, *reqTrack;

  if(!m_master) {
    return;
  }

	int offset = (int)(value - m_master->trackStart().frames(25));
	int trackOffset = doc.trackIndex(doc.findTrack(m_master));

	// in case the master or the track is non-existant, we pretend that it is on the first track.
	if(trackOffset==-1) trackOffset = 0;
	trackOffset = track - trackOffset;

	for(	QValueListIterator<ClipGroupClip> itt = m_clipList.begin(); itt != m_clipList.end(); ++itt) {
		curTrack = (*itt).track;
		if(curTrack) {
			reqTrack = doc.track(doc.trackIndex(curTrack) + trackOffset);
		} else {
			reqTrack = doc.track(trackOffset);
		}

		if(reqTrack==0) {
			kdError() << "Clip Group tried to move clip but couldn't; track to move to does not exist." << endl;
			continue;
		}

		if(curTrack) {
			if(curTrack == reqTrack) {
				(*itt).clip->setTrackStart( GenTime((*itt).clip->trackStart().frames(25) + offset, 25));
			} else {
				curTrack->removeClip((*itt).clip);
				reqTrack->addClip((*itt).clip);
				(*itt).track = reqTrack;   // We have moved the clip; update the track reference in the ClipGroupClip.

				(*itt).clip->setTrackStart( GenTime((*itt).clip->trackStart().frames(25) + offset, 25));
			}
		} else {
			reqTrack->addClip((*itt).clip);
			(*itt).track = reqTrack;
			(*itt).clip->setTrackStart( GenTime((*itt).clip->trackStart().frames(25) + offset, 25));
		}
	}
}

/** Returns an XML representation of this clip group */
QDomDocument ClipGroup::toXML()
{
	QDomDocument doc;

	QValueListIterator<ClipGroupClip> itt;

	for(itt = m_clipList.begin(); itt != m_clipList.end(); ++itt) {
		QDomDocument clipDoc = (*itt).clip->toXML();

		doc.appendChild(doc.importNode(clipDoc.documentElement(), true));
	}

	return doc;
}

/** Returns a url list of all of the clips in this group.*/
KURL::List ClipGroup::createURLList()
{
	KURL::List list;

	QValueListIterator<ClipGroupClip> itt;

	for(itt = m_clipList.begin(); itt != m_clipList.end(); ++itt) {	
	 	list.append((*itt).clip->fileURL());
	}
	
	return list;
}

/** Find the selected clip from within ClipGroup and returns an iterator it.
 Otherwise, returns an iterator to m_clipList.end(). */
QValueListIterator<ClipGroupClip> ClipGroup::findClip(DocClipBase *clip)
{
	QValueListIterator<ClipGroupClip> itt;

	for(itt = m_clipList.begin(); itt != m_clipList.end(); ++itt) {			
		if((*itt).clip == clip) return itt;
	}
	
	return itt;
}

/** Removes a ClipGroupClip from the clipList. This is a private function, as
itterators to the m_clipList should remain inaccessible from outside of the
class. */
void ClipGroup::removeClip(QValueListIterator<ClipGroupClip> &itt)
{
		m_clipList.remove(itt);
		
		if(m_master == (*itt).clip) {
			setMasterClip(0);
		}		
}

/** Deletes all clips in the clip group. This removes the clips from any tracks they are on
 and physically deletes them. After this operation, the clipGroup will be empty. */
void ClipGroup::deleteAllClips()
{
	QValueListIterator<ClipGroupClip> itt;

	while(!m_clipList.isEmpty()) {
		itt = m_clipList.begin();
			
		if((*itt).track) {
			(*itt).track->removeClip((*itt).clip);
		}
		if((*itt).clip) {
			delete (*itt).clip;
		}
		m_clipList.remove(itt);
	}
}
