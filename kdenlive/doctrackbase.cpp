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
		int index = 0;
		DocClipBase *testclip = m_clips.first();
		while(testclip!=0) {
			if(testclip->trackStart() < clip->trackStart()) break;
			
			testclip = m_clips.next();
			index++;
		}
		
		m_clips.insert(index, clip);		
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

/** Returns an iterator to the clip after the last clip(chronologically) which overlaps the start/end
 value range specified.

This allows you to write standard iterator for loops over a specific range of the track. */
QPtrListIterator<DocClipBase> DocTrackBase::endClip(double startValue, double endValue)
{
	QPtrListIterator<DocClipBase> itt(firstClip(startValue, endValue));

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

void DocTrackBase::selectAll()
{
	QPtrListIterator<DocClipBase> itt(m_clips);

	for(DocClipBase *file;	(file=itt.current()) != 0; ++itt) {
		file->setSelected(true);
	}	
}

void DocTrackBase::selectNone()
{
	QPtrListIterator<DocClipBase> itt(m_clips);

	for(DocClipBase *file;	(file=itt.current()) != 0; ++itt) {
		file->setSelected(false);
	}
}

/** Make the clip which exists at the given value selected. Returns true if successful,
    false if no clip exists. */
bool DocTrackBase::selectClipAt(int value)
{
	QPtrListIterator<DocClipBase> itt(m_clips);

	for(DocClipBase *file;	(file=itt.current()) != 0; ++itt) {
		if(file->trackStart() > value) return false;
		if(file->trackStart() + file->cropDuration() > value) {
			file->setSelected(true);
			return true;
		}
	}
	return false;
}

/** Toggles the selected state of the clip at the given value. If it was true, it becomes false,
if it was false, it becomes true. */
bool DocTrackBase::toggleSelectClipAt(int value)
{
	QPtrListIterator<DocClipBase> itt(m_clips);

	for(DocClipBase *file;	(file=itt.current()) != 0; ++itt) {
		if(file->trackStart() > value) return false;
		if(file->trackStart() + file->cropDuration() > value) {
			file->setSelected(!file->isSelected());
			return true;
		}
	}
	return false;
}
