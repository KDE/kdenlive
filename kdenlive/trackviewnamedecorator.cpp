/***************************************************************************
                          trackviewnamedecorator  -  description
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
#include "trackviewnamedecorator.h"

#include <qnamespace.h>
#include  <qpainter.h>

#include "docclipref.h"
#include "gentime.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"

TrackViewNameDecorator::TrackViewNameDecorator(KTimeLine* timeline,
						KdenliveDoc* doc,
						DocTrackBase* track) :
						DocTrackDecorator(timeline, doc, track)
{
}


TrackViewNameDecorator::~TrackViewNameDecorator()
{
}

void TrackViewNameDecorator::paintClip(QPainter &painter, DocClipRef *clip, QRect &rect, bool selected)
{
	int sx = (int)timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));
	int ex = (int)timeline()->mapValueToLocal(clip->trackEnd().frames(document()->framesPerSecond()));
	int clipWidth = ex-sx;
	int tx = ex;

	if(sx < rect.x()) {
		sx = rect.x();
	}
	if(ex > rect.x() + rect.width()) {
		ex = rect.x() + rect.width();
	}
	ex -= sx;

	painter.setClipping(true);
	painter.setClipRect(sx, rect.y(), ex, rect.height());

	// draw video name text
	QRect textBound = painter.boundingRect(0, 0, ex, rect.height(), Qt::AlignLeft, clip->name());

	double border = 50.0;
	int nameRepeat = (int)std::floor((double)clipWidth / ((double)textBound.width() + border));
	if(nameRepeat < 1) nameRepeat = 1;
	int textWidth = clipWidth / nameRepeat;

	if(textWidth > 0) {
		int start = (int)timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));

		start = sx - ((sx - start) % textWidth);
		int count = start;

		while(count < sx+ex) {
			if(count+textWidth <= tx) {
        		painter.setPen( selected ? Qt::white : Qt::black );
				painter.drawText( count, rect.y(), textWidth, rect.height(), Qt::AlignVCenter | Qt::AlignHCenter, clip->name());
        		painter.setPen(Qt::black);
			}
			count += textWidth;
		}
	}

	painter.setClipping(false);
}

