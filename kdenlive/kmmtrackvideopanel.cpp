/***************************************************************************
                          kmmtrackvideopanel.cpp  -  description
                             -------------------
    begin                : Tue Apr 9 2002
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

#include <kdebug.h>
 
#include "kmmtrackvideopanel.h"
#include "kmmtimeline.h"

KMMTrackVideoPanel::KMMTrackVideoPanel(KMMTimeLine &timeline, DocTrackVideo &docTrack, QWidget *parent, const char *name ) :
												KMMTrackPanel(timeline, docTrack, parent,name),
												m_trackLabel(this, "Video Track")
{
	setMinimumWidth(200);
	setMaximumWidth(200);

	setMinimumHeight(50);
	setMaximumHeight(50);		
	setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding));
}

KMMTrackVideoPanel::~KMMTrackVideoPanel()
{
}

/** Paint the specified clip on screen within the specified rectangle, using the specified painter. */
void KMMTrackVideoPanel::paintClip(QPainter &painter, DocClipBase *clip, QRect &rect, bool selected)
{	
	int sx = (int)timeLine().mapValueToLocal(clip->trackStart());
	int ex = (int)timeLine().mapValueToLocal(clip->trackStart() + clip->cropDuration());
	
	if(sx < rect.x()) {
		sx = rect.x();
	}
	if(ex > rect.x() + rect.width()) {
		ex = rect.x() + rect.width();
	}		
	ex -= sx;

	QColor col = selected ? QColor(128, 64, 64) : QColor(255, 128, 128);

	painter.fillRect( sx, rect.y(),
										ex, rect.height(),
										col);
	painter.drawRect( sx, rect.y(),
										ex, rect.height());

	QRect textBound = painter.boundingRect(0, 0, ex, rect.height(), AlignLeft, clip->name());

	int textWidth = textBound.width() + 100;

	int start = (int)timeLine().mapValueToLocal(clip->trackStart());
	start += 25;

	start = sx - ((sx - start) % textWidth);
	int count = start;	

	while(count < ex+start) {
		painter.drawText( count, rect.y(), ex, rect.height(), AlignVCenter | AlignLeft, clip->name());
		count += textWidth;
	}
}
