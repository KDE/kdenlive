/***************************************************************************
                          trackviewdecorator  -  description
                             -------------------
    begin                : Thu Nov 27 2003
    copyright            : (C) 2003 by Jason Wood
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
#include "trackviewdecorator.h"

#include <qptrlist.h>

#include "doctrackbase.h"
#include "gentime.h"
#include "kdenlivedoc.h"
#include "kmmtimeline.h"

TrackViewDecorator::TrackViewDecorator(KMMTimeLine *timeline, KdenliveDoc *doc, DocTrackBase *track) :
							m_timeline(timeline),
							m_document(doc),
							m_docTrack(track)
{
}


TrackViewDecorator::~TrackViewDecorator()
{
}

void TrackViewDecorator::drawToBackBuffer(QPainter &painter, QRect &rect)
{
	GenTime startValue = GenTime(m_timeline->mapLocalToValue(0.0), m_document->framesPerSecond());
	GenTime endValue = GenTime(m_timeline->mapLocalToValue(rect.width()), m_document->framesPerSecond());

	QPtrListIterator<DocClipRef> clip = m_docTrack->firstClip(startValue, endValue, false);
	DocClipRef *endClip = m_docTrack->endClip(startValue, endValue, false).current();
	for(DocClipRef *curClip; (curClip = clip.current())!=endClip; ++clip) {
		paintClip(painter, curClip, rect, false);
	}

	clip = m_docTrack->firstClip(startValue, endValue, true);
	endClip = m_docTrack->endClip(startValue, endValue, true).current();
	for(DocClipRef *curClip; (curClip = clip.current())!=endClip; ++clip) {
		paintClip(painter, curClip, rect, true);
	}
}
