/***************************************************************************
                          kmmtracksoundpanel.cpp  -  description
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

#include <qsizepolicy.h>

#include "kmmtracksoundpanel.h"
#include "kmmtimeline.h"

#include "trackviewbackgrounddecorator.h"
#include "trackviewnamedecorator.h"

KMMTrackSoundPanel::KMMTrackSoundPanel(KMMTimeLine *timeline, 
					KdenliveDoc *document,
					DocTrackSound *docTrack, 
					QWidget *parent, 
					const char *name ) :
						KMMTrackPanel(timeline, document, docTrack, parent,name),
						m_trackLabel(this, "Sound Track")
{
	setMinimumHeight(20);
	setMaximumHeight(20);

	addViewDecorator(new TrackViewBackgroundDecorator(timeline, document, docTrack, QColor(64, 128, 64), QColor(128, 255, 128)));
	addViewDecorator(new TrackViewNameDecorator(timeline, document, docTrack));
}

KMMTrackSoundPanel::~KMMTrackSoundPanel()
{
}

/*
void KMMTrackSoundPanel::paintClip(QPainter & painter, DocClipRef * clip, QRect &rect, bool selected)
{
	int sx = (int)timeLine()->mapValueToLocal(clip->trackStart().frames(25));
	int ex = (int)timeLine()->mapValueToLocal(clip->trackStart().frames(25) + clip->cropDuration().frames(25));

	if(sx < rect.x()) {
		sx = rect.x();
	}
	if(ex > rect.x() + rect.width()) {
		ex = rect.x() + rect.width();
	}
	ex -= sx;

	QColor col = selected ? QColor(64, 128, 64) : QColor(128, 255, 128);

	painter.fillRect( sx, rect.y(),
										ex, rect.height(),
										col);
	painter.drawRect( sx, rect.y(),
										ex, rect.height());
}
*/
