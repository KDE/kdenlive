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
