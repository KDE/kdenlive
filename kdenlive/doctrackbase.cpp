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

DocTrackBase::DocTrackBase()
{
}

DocTrackBase::~DocTrackBase()
{
}

/** Adds a clip to the track. Returns true if successful, false if it fails for some reason.

This method calls canAddClip() to determine whether or not the clip can be added to this
particular track. */
bool DocTrackBase::addClip(DocClipBase *clip)
{
	if(canAddClip(clip)) {
		m_clips.inSort(clip);
		clip->setParentTrack(this);
		emit trackChanged();
		return true;
	} else {
		return false;
	}
}

/** Returns an iterator to the first clip (chronologically) which overlaps the start/end value range specified. */
QPtrListIterator<DocClipBase> DocTrackBase::firstClip(double startValue, double endValue)
{
	QPtrListIterator<DocClipBase> itt(m_clips);

	DocClipBase *clip;
	
	if(itt.isEmpty()) return itt;

	while( (clip = itt.current())	!= 0) {
		if(clip->trackStart().frames(25) > endValue) {
			// out of range, return iterator with a current() value of null.
			itt.toLast();
			++itt;
			return itt;
		}
		if(clip->trackStart().frames(25) + clip->cropDuration().frames(25) >= startValue) {
			if(clip->trackStart().frames(25) <= endValue) {			
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

/** Returns an iterator to the clip after the last clip(chronologically) which overlaps the start/end
 value range specified.

This allows you to write standard iterator for loops over a specific range of the track. */
QPtrListIterator<DocClipBase> DocTrackBase::endClip(double startValue, double endValue)
{
	QPtrListIterator<DocClipBase> itt(firstClip(startValue, endValue));

	DocClipBase *clip;

	if(itt.isEmpty()) return itt;

	while( (clip = itt.current())	!= 0) {
		if(clip->trackStart().frames(25) > endValue) {
			return itt;			
		}
		++itt;
	}
	
	return itt;
}

DocClipBase *DocTrackBase::getClipAt(int value)
{
	QPtrListIterator<DocClipBase> itt(m_clips);

	for(DocClipBase *file;	(file=itt.current()) != 0; ++itt) {
		if(file->trackStart().frames(25) > value) return false;
		if(file->trackStart().frames(25) + file->cropDuration().frames(25) > value) {
			return file;
		}
	}
	
	return 0;
}

/** returns true if all of the clips within the cliplist can be added, returns false otherwise. */
bool DocTrackBase::canAddClips(DocClipBaseList clipList)
{
	QPtrListIterator<DocClipBase> itt(clipList);

	for(DocClipBase *clip; (clip = itt.current())!=0; ++itt) {
		if(!canAddClip(clip)) return false;
	}

	return true;
}

/** Adds all of the clips in the pointerlist into this track. */
void DocTrackBase::addClips(DocClipBaseList list)
{
	QPtrListIterator<DocClipBase> itt(list);

	for(DocClipBase *clip; (clip = itt.current()) !=- 0; ++itt) {
		addClip(clip);
	}
}

/** Returns true if the clip given exists in this track, otherwise returns
false. */
bool DocTrackBase::clipExists(DocClipBase *clip)
{
	return (m_clips.find(clip)!=-1);
}

/** Removes the given clip from this track. If it doesn't exist, then
a warning is issued vi kdWarning, but otherwise no bad things
will happen. The clip is removed from the track, but NOT deleted. */
void DocTrackBase::removeClip(DocClipBase *clip)
{
	if(!m_clips.remove(clip)) {
		kdError() << "Cannot remove clip from track - doesn't exist!" << endl;
	} else {
		clip->setParentTrack(0);
	}
}

/** The clip specified has moved. This method makes sure that the clips
are still in the correct order, rearranging them if they are not. */
void DocTrackBase::clipMoved(DocClipBase *clip)
{
	// sanity check
	if(!clip) {
		kdError() << "TrackBase has been alerted that a clip has moved... but no clip specified!" << endl;
		return;
	}
	int pos;

	if((pos = m_clips.find(clip))==-1) {
		kdError() << "Track told that non-existant clip has moved (that's gotta be a bug!)" << endl;
		return;
	}

	m_clips.take(pos);
	m_clips.inSort(clip);

	DocClipBase *t = m_clips.first();

	while(t) {
		t = m_clips.next();
	}
}
