/***************************************************************************
                          kclipplacer  -  description
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
#include "kclipplacer.h"

#include "docclipref.h"
#include "ktimeline.h"
#include "trackviewdecorator.h"

namespace Gui
{

KClipPlacer::KClipPlacer(KTimeLine *timeline, DocClipRef *clip) :
					KPlacer(),
					m_clip(clip),
					m_timeline(timeline)
{
}


KClipPlacer::~KClipPlacer()
{
}

// virtual
void KClipPlacer::drawToBackBuffer(QPainter& painter, QRect& rect, TrackViewDecorator *decorator)
{
	double sx = m_timeline->mapValueToLocal(0);
	double ex = m_timeline->mapValueToLocal(m_clip->cropDuration().frames(m_clip->framesPerSecond()));
	decorator->paintClip(sx, ex, painter, m_clip, rect, false);
}

/** Returns the track index into the underlying document model used by this track. Returns -1 if this is inapplicable. */
//  virtual
int KClipPlacer::documentTrackIndex()  const
{
	return -1;
}

} // namespace Gui
