/***************************************************************************
                          trackviewbackgrounddecorator  -  description
                             -------------------
    begin                : Fri Nov 28 2003
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
#include "trackviewbackgrounddecorator.h"

#include <qpainter.h>

#include "docclipref.h"
#include "gentime.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"

TrackViewBackgroundDecorator::TrackViewBackgroundDecorator(KTimeLine* timeline,
												KdenliveDoc* doc,
												DocTrackBase* track,
												const QColor &selected,
												const QColor &unselected) :
									DocTrackDecorator(timeline, doc, track),
									m_selected(selected),
									m_unselected(unselected)
{
}


TrackViewBackgroundDecorator::~TrackViewBackgroundDecorator()
{
}


void TrackViewBackgroundDecorator::paintClip(QPainter& painter, DocClipRef* clip, QRect& rect, bool selected)
{
	int sx = (int)timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));
	int ex = (int)timeline()->mapValueToLocal(clip->trackEnd().frames(document()->framesPerSecond()));

	if(sx < rect.x()) {
		sx = rect.x();
	}
	if(ex > rect.x() + rect.width()) {
		ex = rect.x() + rect.width();
	}
	ex -= sx;

	QColor col = selected ? m_selected : m_unselected;

	// draw outline box
	painter.fillRect( sx, rect.y(), ex, rect.height(), col);

	painter.drawRect( sx, rect.y(), ex, rect.height());
}

