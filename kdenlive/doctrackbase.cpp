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
#include "doctrackvideo.h"
#include "doctracksound.h"
#include "doctrackclipiterator.h"
#include "kdenlivedoc.h"

DocTrackBase::DocTrackBase(KdenliveDoc *doc)
{
	m_sortingEnabled = 1;
	m_collisionDetectionEnabled = 1;
	m_doc = doc;
	m_selectedClipList.setAutoDelete(false);
	m_unselectedClipList.setAutoDelete(false);	
}

DocTrackBase::~DocTrackBase()
{
	m_selectedClipList.setAutoDelete(true);
	m_unselectedClipList.setAutoDelete(true);
}

bool DocTrackBase::addClip(DocClipBase *clip, bool selected)
{
	if(canAddClip(clip)) {
		if(selected) {
			m_selectedClipList.inSort(clip);
      emit signalClipSelected(clip);
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

	while( (file = u_itt.current()) ) {
		if(file->trackStart() > value) break;
		if(file->trackEnd() >= value) {
			return file;
		}
		++u_itt;
	}

	QPtrListIterator<DocClipBase> s_itt(m_selectedClipList);
	while( (file = s_itt.current()) ) {
		if(file->trackStart() > value) break;
		if(file->trackEnd() >= value) {
			return file;
		}
		++s_itt;
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
	return ((m_unselectedClipList.findRef(clip)!=-1) || (m_selectedClipList.findRef(clip)!=-1));
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
	emit trackChanged();	
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
	emit trackChanged();
}

void DocTrackBase::selectClip(DocClipBase *clip, bool select)
{
	removeClip(clip);
	addClip(clip, select);
}

bool DocTrackBase::hasSelectedClips()
{
	return (!m_selectedClipList.isEmpty());
}

QPtrListIterator<DocClipBase> DocTrackBase::firstClip(bool selected) const
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

		clip->moveTrackStart(clip->trackStart() + offset);
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

	emit trackChanged();
  return returnList;
}

void DocTrackBase::deleteClips(bool selected)
{
	DocClipBaseList *list = &(selected ? m_selectedClipList : m_unselectedClipList);

	list->setAutoDelete(true);
	list->clear();
	list->setAutoDelete(false);
	emit trackChanged();    	
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
		newStart = clip->cropDuration() - clip->duration();
	}

	if(clip->cropDuration() - newStart < 0) {
		kdWarning() << "Clip cannot be resized to length < 1 frame, fixing..." << endl;
		newStart = clip->cropDuration() - GenTime(1, m_doc->framesPerSecond());
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
}

void DocTrackBase::resizeClipTrackEnd(DocClipBase *clip, GenTime newEnd)
{
	if(!clipExists(clip)) {
		kdError() << "Trying to resize non-existant clip! (resizeClipCropDuration)" << endl;
		return;
	}	

	GenTime cropDuration = newEnd - clip->trackStart();

	if(cropDuration > clip->duration() - clip->cropStartTime()) {
		newEnd = clip->duration() - clip->cropStartTime() + clip->trackStart();
		cropDuration = newEnd - clip->trackStart();
	}
	
	if(newEnd < clip->trackStart()) {
		kdWarning() << "Clip cannot be resized to < 1 frame in size, fixing..." << endl;
		newEnd = clip->trackStart() + GenTime(1, m_doc->framesPerSecond());
	}

	#warning - the following code does not work for large increments - small clips might be overlapped.
	#warning - Replace with code that looks at the clip directly after the clip we are working with.
	DocClipBase *test;

	test = getClipAt(newEnd); 

	if((test) && (test != clip)) {
		newEnd = test->trackStart();
	}

	clip->setTrackEnd(newEnd);
}

/** Returns the total length of the track - in other words, it returns the end of the
last clip on the track. */
GenTime DocTrackBase::trackLength()
{
	GenTime slength;	
	GenTime ulength;

	if(!m_selectedClipList.isEmpty()) {
		slength = m_selectedClipList.last()->trackStart() + m_selectedClipList.last()->cropDuration();		
	}

	if(!m_unselectedClipList.isEmpty()) {
		ulength = m_unselectedClipList.last()->trackStart() + m_unselectedClipList.last()->cropDuration();	
	}

	return (slength > ulength) ? slength : ulength;
}

/** Returns the number of clips contained in this track. */
unsigned int DocTrackBase::numClips()
{
	return m_selectedClipList.count() + m_unselectedClipList.count();
}

/** Returns the parent document of this track. */
KdenliveDoc * DocTrackBase::document()
{
	return m_doc;
}

/** Returns an xml representation of this track. */
QDomDocument DocTrackBase::toXML()
{
	QDomDocument doc;

	doc.appendChild(doc.createElement("track"));
	doc.documentElement().setAttribute("cliptype", clipType());

  DocTrackClipIterator itt(*this);

  while(itt.current()) {
		doc.documentElement().appendChild(doc.importNode(itt.current()->toXML().documentElement(), true));  
  	++itt;
  }

	return doc;
}

/** Creates a track from the given xml document. Returns the track, or 0 if it could not be created. */
DocTrackBase * DocTrackBase::createTrack(KdenliveDoc *doc, QDomElement elem)
{
	if(elem.tagName() != "track") {
		kdError() << "Cannot create track from QDomElement - has wrong tag name : " << elem.tagName() << endl;
		return 0;
	}

	QString clipType = elem.attribute("cliptype", "unknown");

	DocTrackBase *track;

	if(clipType == "Video") {
		track = new DocTrackVideo(doc);
	} else if(clipType == "Sound") {
		track = new DocTrackSound(doc);
	} else {
		kdError() << "Unknown track clip type '" << clipType << "' - cannot create track" << endl;
		return 0;
	}

	QDomNode n = elem.firstChild();

	while(!n.isNull()) {
		QDomElement e = n.toElement();
		if(!e.isNull()) {
			if(e.tagName() == "clip") {
				DocClipBase *clip = DocClipBase::createClip(doc, e);
				if(clip) {
					track->addClip(clip, false);
				} else {
					kdWarning() << "Clip generation failed, skipping clip..." << endl;
				}
			} else {
				kdWarning() << "Unknown tag " << e.tagName() << ", skipping..." << endl;
			}
		}

		n = n.nextSibling();
	}	

	return track;
}

/** Alerts the track that it's trackIndex within the document has
changed. The track should update the clips on it with the new
index value. */
void DocTrackBase::trackIndexChanged(int index)
{
  DocTrackClipIterator itt(*this);

  while(itt.current()) {
  	itt.current()->setParentTrack(this, index);
  	++itt;
  }
}
