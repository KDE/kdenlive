/***************************************************************************
                          ktrackplacer  -  description
                             -------------------
    begin                : Tue Apr 6 2004
    copyright            : (C) 2004 by Jason Wood
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
#include "ktrackplacer.h"
#include "ktimeline.h"

#include "kdenlivedoc.h"
#include "doctrackbase.h"
#include "trackviewdecorator.h"

KTrackPlacer::KTrackPlacer(KdenliveDoc *doc, KTimeLine *timeline, DocTrackBase *track) :
				m_docTrack(track),
				m_timeline(timeline),
				m_document(doc)
{
}


KTrackPlacer::~KTrackPlacer()
{
}

// virtual
void KTrackPlacer::drawToBackBuffer(QPainter &painter, QRect &rect, TrackViewDecorator *decorator)
{
	GenTime startValue = GenTime(m_timeline->mapLocalToValue(0.0), m_docTrack->framesPerSecond());
	GenTime endValue = GenTime(m_timeline->mapLocalToValue(rect.width()), m_docTrack->framesPerSecond());

	QPtrListIterator<DocClipRef> clip = m_docTrack->firstClip(startValue, endValue, false);
	DocClipRef *endClip = m_docTrack->endClip(startValue, endValue, false).current();
	for(DocClipRef *curClip; (curClip = clip.current())!=endClip; ++clip) {
		double sx = m_timeline->mapValueToLocal(curClip->trackStart().frames(m_docTrack->framesPerSecond()));
		double ex = m_timeline->mapValueToLocal(curClip->trackEnd().frames(m_docTrack->framesPerSecond()));
		decorator->paintClip(sx, ex, painter, curClip, rect, false);
	}

	clip = m_docTrack->firstClip(startValue, endValue, true);
	endClip = m_docTrack->endClip(startValue, endValue, true).current();
	for(DocClipRef *curClip; (curClip = clip.current())!=endClip; ++clip) {
		double sx = m_timeline->mapValueToLocal(curClip->trackStart().frames(m_docTrack->framesPerSecond()));
		double ex = m_timeline->mapValueToLocal(curClip->trackEnd().frames(m_docTrack->framesPerSecond()));
		decorator->paintClip(sx, ex, painter, curClip, rect, true);
	}
}

int KTrackPlacer::documentTrackIndex()  const
{
	return m_document->trackIndex(m_docTrack);
}
