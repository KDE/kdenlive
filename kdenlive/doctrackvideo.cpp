/***************************************************************************
                          doctrackvideo.cpp  -  description
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

#include "doctrackvideo.h"

#include "kdebug.h"

#include "avfile.h"
#include "docclipavfile.h"

DocTrackVideo::DocTrackVideo(DocClipProject * project):
DocTrackBase(project)
{
}

DocTrackVideo::~DocTrackVideo()
{
}

/** Returns true if the specified clip can be added to this track, false otherwise.*/
bool DocTrackVideo::canAddClip(DocClipRef * clip) const
{
    DocClipRef *search;

    if (!clip)
	return false;

    QPtrListIterator < DocClipRef > u_itt(m_unselectedClipList);
    for (; (search = u_itt.current()) != 0; ++u_itt) {
	if (search->trackEnd() <= clip->trackStart())
	    continue;
	if (search->trackStart() < clip->trackEnd()) {
	    return false;
	}
	// we can safely break here, as the clips are sorted in order - if search->trackStart is already past
	// the clip that we was looking at, then we are ok.
	break;
    }

    // repeated for selected clips
    QPtrListIterator < DocClipRef > s_itt(m_selectedClipList);
    for (; (search = s_itt.current()) != 0; ++s_itt) {
	if (search->trackEnd() <= clip->trackStart())
	    continue;
	if (search->trackStart() < clip->trackEnd()) {
	    kdDebug() << "Cannot add clip at " << clip->trackStart().
		seconds() << " to " << clip->trackEnd().seconds() << endl;
	    kdDebug() << "Because of clip " << search->trackStart().
		seconds() << " to " << search->trackEnd().
		seconds() << endl;
	    return false;
	}
	// we can safely break here, as the clips are sorted in order - if search->trackStart is already past
	// the clip that we was looking at, then we are ok.
	break;
    }

    return true;
}



/** Returns the clip type as a string. This is a bit of a hack to give the
		* KMMTimeLine a way to determine which class it should associate
		*	with each type of clip. */
const QString & DocTrackVideo::clipType() const
{
    static const QString clipType = "Video";
    return clipType;
}
