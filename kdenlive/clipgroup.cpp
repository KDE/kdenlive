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
	m_clipList.setAutoDelete(false);
	m_master = 0;
}

ClipGroup::~ClipGroup()
{
}

/** Adds a clip to the clip group. If the clip is already in the group, it will not be added a second time. */
void ClipGroup::addClip(DocClipBase *clip)
{
	if(!clipExists(clip)) {
		m_clipList.append(clip);
		setMasterClip(clip);
	}
}

/** Removes the selected clip from the clip group. If the clip passed is not in the clip group, nothing (other than a warning) will happen. */
void ClipGroup::removeClip(DocClipBase *clip)
{
	if(clipExists(clip)) {
		m_clipList.remove(clip);
		if(m_master==clip) {
			setMasterClip(NULL);
		}
	}
}

/** Returns true if the specified clip is in the group. */
bool ClipGroup::clipExists(DocClipBase *clip)
{
	return (m_clipList.find(clip)!=-1);
}

/** Removes the clip if it is in the list, adds it if it isn't. Returns true if the clip is now part of the list, false if it is not. */
bool ClipGroup::toggleClip(DocClipBase *clip)
{
	if(clipExists(clip)) {
		removeClip(clip);
		return false;
	}

	addClip(clip);
	return true;
}

/** Moves all clips by the offset specified. Returns true if successful, fasle on failure. */
bool ClipGroup::moveByOffset(int offset)
{
	QPtrListIterator<DocClipBase> itt(m_clipList);

	DocClipBase *clip;
	
	while( (clip = itt.current())) {
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
	if(m_master==NULL) return false;

	newValue = value;
	
	return true;
}

/** Returns a clip list of all clips contained within the group. */
QPtrList<DocClipBase> ClipGroup::clipList()
{
	return m_clipList;
}

/** returns true is this clipGroup is currently empty, false otherwise. */
bool ClipGroup::isEmpty()
{
	return m_clipList.count()==0;
}

void ClipGroup::setMasterClip(DocClipBase *master)
{
	if((master==NULL) || (!clipExists(master))) {
		if(m_clipList.isEmpty()) {
			m_master = NULL;
		} else {
			m_master = m_clipList.first();
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
	QPtrListIterator<DocClipBase> itt(m_clipList);

	DocTrackBase *curTrack, *reqTrack;

  if(!m_master) {
    return;
  }
    
	int offset = (int)(value - m_master->trackStart().frames(25));
	
	for(DocClipBase *clip; (clip = itt.current()); ++itt) {
		curTrack = doc.findTrack(clip);
		reqTrack = doc.track(track);

		if(reqTrack==0) {
			kdError() << "Clip Group tried to move clip but couldn't; track to move to does not exist." << endl;
			continue;
		}		
		
		if(curTrack) {
			
			if(curTrack == reqTrack) {
				clip->setTrackStart( GenTime(clip->trackStart().frames(25) + offset, 25));
			} else {
				curTrack->removeClip(clip);
				reqTrack->addClip(clip);
				clip->setTrackStart( GenTime(clip->trackStart().frames(25) + offset, 25));
			}
			
		} else {
			reqTrack->addClip(clip);
			clip->setTrackStart( GenTime(clip->trackStart().frames(25) + offset, 25));
		}	
	}
}

/** Returns an XML representation of this clip group */
QDomDocument ClipGroup::toXML()
{
	QDomDocument doc;

	QPtrListIterator<DocClipBase> itt(m_clipList);		

	for(DocClipBase *clip; (clip=itt.current()); ++itt) {
		QDomDocument clipDoc = clip->toXML();

		doc.appendChild(doc.importNode(clipDoc.documentElement(), true));		
	}

	return doc;
}

/** Returns a url list of all of the clips in this group.*/
KURL::List ClipGroup::createURLList()
{
	KURL::List list;

	QPtrListIterator<DocClipBase> itt(m_clipList);

	for(DocClipBase *clip; (clip = itt.current()); ++itt) {	
	 	list.append(clip->fileURL());
	}
	
	return list;
}
