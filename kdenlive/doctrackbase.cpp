/***************************************************************************
                          doctrackbase.cpp  -  description
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

#include "kdebug.h"
#include "doctrackbase.h"
#include "kdenlivedoc.h"

DocTrackBase::DocTrackBase(KdenliveDoc *doc)
{
	m_sortingEnabled = 1;
	m_collisionDetectionEnabled = 1;
	m_doc = doc;
}

DocTrackBase::~DocTrackBase()
{
}

bool DocTrackBase::addClip(DocClipBase *clip, bool selected)
{
	if(canAddClip(clip)) {
		if(selected) {
			m_selectedClipList.inSort(clip);		
		} else {
			m_unselectedClipList.inSort(clip);			
		}
		clip->setParentTrack(this, m_doc->trackIndex(this));
		emit trackChanged();
		return true;
	} else {
		return false;
	}
}

QPtrListIterator<DocClipBase> DocTrackBase::firstClip(GenTime startValue, GenTime endValue, bool selected)
{	
	QPtrListIterator<DocClipBase> itt( selected ? m_selectedClipList : m_unselectedClipList);

	DocClipBase *clip;
	
	if(itt.isEmpty()) return itt;

	while( (clip = itt.current())	!= 0) {
		if(clip->trackStart() > endValue) {
			// out of range, return iterator with a current() value of null.
			itt.toLast();
			++itt;
			return itt;
		}
		if(clip->trackStart() + clip->cropDuration() >= startValue) {
			if(clip->trackStart() <= endValue) {			
				// this clip is at least partially on screen.
				return itt;
			} else {
				// this clip is not on screen, return iterator with current() value of null
				itt.toLast();
				++itt;
				return itt;
			}
		}
		++itt;
		
	}
	
	return itt;
}

QPtrListIterator<DocClipBase> DocTrackBase::endClip(GenTime startValue, GenTime endValue, bool selected)
{
	QPtrListIterator<DocClipBase> itt(firstClip(startValue, endValue, selected));

	DocClipBase *clip;

	if(itt.isEmpty()) return itt;

	while( (clip = itt.current())	!= 0) {
		if(clip->trackStart() > endValue) {
			return itt;			
		}
		++itt;
	}
	
	return itt;
}

DocClipBase *DocTrackBase::getClipAt(GenTime value)
{
	QPtrListIterator<DocClipBase> u_itt(m_unselectedClipList);
	DocClipBase *file;
	
	for(;	(file=u_itt.current()) != 0; ++u_itt) {
		if(file->trackStart() > value) break;
		if(file->trackStart() + file->cropDuration() > value) {
			return file;
		}
	}

	QPtrListIterator<DocClipBase> s_itt(m_selectedClipList);	
	for(;	(file=s_itt.current()) != 0; ++s_itt) {
		if(file->trackStart() > value) break;
		if(file->trackStart() + file->cropDuration() > value) {
			return file;
		}
	}

	return 0;
}

bool DocTrackBase::canAddClips(DocClipBaseList clipList)
{
	QPtrListIterator<DocClipBase> itt(clipList);

	for(DocClipBase *clip; (clip = itt.current())!=0; ++itt) {
		if(!canAddClip(clip)) return false;
	}

	return true;
}

void DocTrackBase::addClips(DocClipBaseList list, bool selected)
{
	QPtrListIterator<DocClipBase> itt(list);

	for(DocClipBase *clip; (clip = itt.current()) !=- 0; ++itt) {
		addClip(clip, selected);
	}
}

bool DocTrackBase::clipExists(DocClipBase *clip)
{
	return ((m_unselectedClipList.find(clip)!=-1) || (m_selectedClipList.find(clip)!=-1));
}

bool DocTrackBase::removeClip(DocClipBase *clip)
{
	if(!clip) return false;
		
	if((!m_selectedClipList.remove(clip)) && (!m_unselectedClipList.remove(clip))) {		
		kdError() << "Cannot remove clip from track - doesn't exist!" << endl;
		return false;
	} else {
		clip->setParentTrack(0, -1);
	}
	return true;
}

void DocTrackBase::clipMoved(DocClipBase *clip)
{
	if(m_sortingEnabled < 1) return;	// Don't sort if we aren't supposed to.yet.
	
	// sanity check
	if(!clip) {
		kdError() << "TrackBase has been alerted that a clip has moved... but no clip specified!" << endl;
		return;
	}

	int pos;
	DocClipBaseList *list = &m_selectedClipList;
	pos = list->find(clip);
	if(pos==-1) {
		list = &m_unselectedClipList;
		pos = list->find(clip);
		if(pos==-1) {
			kdError() << "Track told that non-existant clip has moved (that's gotta be a bug!)" << endl;
			return;
		}
	}

	list->take(pos);
	list->inSort(clip);
}

void DocTrackBase::selectClip(DocClipBase *clip)
{
	removeClip(clip);
	addClip(clip, true);
}

bool DocTrackBase::hasSelectedClips()
{
	return (!m_selectedClipList.isEmpty());
}

QPtrListIterator<DocClipBase> DocTrackBase::firstClip(bool selected)
{
	return QPtrListIterator<DocClipBase>(selected ? m_selectedClipList : m_unselectedClipList);
}

void DocTrackBase::moveClips(GenTime offset, bool selected)
{
	enableClipSorting(false);
	enableCollisionDetection(false);

	QPtrListIterator<DocClipBase> itt((selected) ? m_selectedClipList : m_unselectedClipList);

	DocClipBase *clip;

	while( (clip=itt.current()) != 0) {
		++itt;

		clip->setTrackStart(clip->trackStart() + offset);		
	}
	
	enableCollisionDetection(true);
	enableClipSorting(true);
}

void DocTrackBase::enableClipSorting(bool enabled)
{
	m_sortingEnabled += (enabled) ? 1 : -1;
}

void DocTrackBase::enableCollisionDetection(bool enable)
{
	m_collisionDetectionEnabled += (enable) ? 1 : -1;
}

DocClipBaseList DocTrackBase::removeClips(bool selected)
{
	DocClipBaseList &list = selected ? m_selectedClipList : m_unselectedClipList;
  DocClipBaseList returnList;

  while(!list.isEmpty()) {
  	DocClipBase *clip = list.first();
   	// When we are removing clips and placing them into a list, it is likely that we are removing clips from
    // a number of tracks and putting them into a single list elsewhere. We must wipe the parent track pointer,
    // but by keeping the track hint, we make it easier to relocate the clips on a different timeline or some
    // other similar operation. So, the following line, wipes the clip's parent track but keeps the trackNum.
   	clip->setParentTrack(0, clip->trackNum());
   	list.removeFirst();    
   	returnList.append(clip);
  }

  return returnList;
}

void DocTrackBase::deleteClips(bool selected)
{
	DocClipBaseList *list = &(selected ? m_selectedClipList : m_unselectedClipList);

	list->setAutoDelete(true);
	list->clear();
	list->setAutoDelete(false);
}

void DocTrackBase::selectNone()
{
	// Neat and compact, but maybe confusing.
	// This says "Remove all of the clips that are currently selected from the track and put them into a list.
	// Then, add them to the track again as non-selected clips.
	addClips(removeClips(true), false);	
}

void DocTrackBase::toggleSelectClip(DocClipBase *clip)
{
	if(!clip) {
		kdError() << "Trying to toggleSelect null clip!" << endl;
		return;
	}
	int num = m_selectedClipList.find(clip);
	if(num!=-1) {
		m_selectedClipList.take(num);
		m_unselectedClipList.inSort(clip);
	} else {
		num = m_unselectedClipList.find(clip);
		if(num!=-1) {
			m_unselectedClipList.take(num);
			m_selectedClipList.inSort(clip);
		} else {
			kdWarning() << "Cannot toggleSelectClip() - clip not on track!" << endl;
		}
	}
}

bool DocTrackBase::clipSelected(DocClipBase *clip)
{
	return (m_selectedClipList.find(clip) != -1);
}

void DocTrackBase::resizeClipTrackStart(DocClipBase *clip, GenTime newStart)
{
	if(!clipExists(clip)) {
		kdError() << "Trying to resize non-existant clip! (resizeClipTrackStart)" << endl;
		return;		
	}

	newStart = newStart - clip->trackStart();

	if(clip->cropStartTime() + newStart < 0) {
		newStart = GenTime() - clip->cropStartTime();
	}

	if(clip->cropDuration() - newStart > clip->duration()) {
		newStart = clip->duration() + clip->cropDuration();
	}

	if(clip->cropDuration() - newStart < 0) {
		newStart = clip->cropDuration();
		kdWarning() << "Clip resized to zero length!" << endl;
	}

	#warning - the following code does not work for large increments - small clips might be overlapped.
	#warning - Replace with code that looks at the clip directly before the clip we are working with.
	DocClipBase *test;
	test = getClipAt(clip->trackStart() + newStart);

	if((test) && (test != clip)) {
		newStart = test->trackStart() + test->cropDuration() - clip->trackStart();
	}

	clip->setTrackStart(clip->trackStart() + newStart);
	clip->setCropStartTime(clip->cropStartTime() + newStart);
	clip->setCropDuration(clip->cropDuration() - newStart);
}

void DocTrackBase::resizeClipCropDuration(DocClipBase *clip, GenTime newStart)
{
	if(!clipExists(clip)) {
		kdError() << "Trying to resize non-existant clip! (resizeClipCropDuration)" << endl;
		return;
	}	
	
	newStart = newStart - clip->trackStart();

	if(newStart > clip->duration() - clip->cropStartTime()) {
		newStart = clip->duration() - clip->cropStartTime();
	}
	
	if(newStart < clip->cropStartTime()) {
		kdWarning() << "Clip has been resized to zero length" << endl;
		newStart = clip->cropStartTime();
	}

	#warning - the following code does not work for large increments - small clips might be overlapped.
	#warning - Replace with code that looks at the clip directly after the clip we are working with.
	DocClipBase *test;

	test = getClipAt(clip->trackStart() + newStart); 

	if((test) && (test != clip)) {
		newStart = test->trackStart() - clip->trackStart();
	}

	clip->setCropDuration(newStart);
}
