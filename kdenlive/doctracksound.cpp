/***************************************************************************
                          doctracksound.cpp  -  description
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
 
#include "doctracksound.h"
#include "docclipavfile.h"

DocTrackSound::DocTrackSound() :
					DocTrackBase()
{
}

DocTrackSound::~DocTrackSound()
{
}

/** Returns true if the specified clip can be added to this track, false otherwise. */
bool DocTrackSound::canAddClip(DocClipBase * clip)
{
	QPtrListIterator<DocClipBase> itt(m_clips);

	for(DocClipBase *search; (search=itt.current()) != 0; ++itt) {
		if(search->trackStart() + search->cropDuration() < clip->trackStart()) continue;
		if(search->trackStart() < clip->trackStart() + clip->cropDuration()) return false;
		// we can safely break here, as the clips are sorted in order - if search->trackStart is already past
		// the clip that we was looking at, then we are ok.
		break;
	}
	
	return true;
}
/** Returns the clip type as a string. This is a bit of a hack to give the
		* KMMTimeLine a way to determine which class it should associate
		*	with each type of clip. */
QString DocTrackSound::clipType()
{
	return "Sound";
}
