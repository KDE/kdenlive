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

#include "kmmtrackvideopanel.h"

KMMTrackVideoPanel::KMMTrackVideoPanel(KMMTimeLine &timeline, DocTrackVideo &docTrack, QWidget *parent, const char *name ) :
												KMMTrackPanel(timeline, docTrack, parent,name),
												m_trackLabel(this, "Video Track")
{
	setMinimumWidth(200);
	setMaximumWidth(200);
	setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding));
}

KMMTrackVideoPanel::~KMMTrackVideoPanel()
{
}

/** Paint the specified clip on screen within the specified rectangle, using the specified painter. */
void KMMTrackVideoPanel::paintClip(QPainter &painter, DocClipBase *clip)
{
	int sx = (int)timeLine().mapValueToLocal(clip->trackStart());
	int ex = (int)timeLine().mapValueToLocal(clip->cropDuration());
	if(sx < 0) {
		ex += sx;
		sx = 0;
	}
	if(sx + ex > width()) {
		ex = width() - sx;
	}

  QColor col = QColor(255, 128, 128);

	if(clip->isSelected()) {
		col = QColor(128, 64, 64);
	}

	painter.fillRect( sx, 0,
										ex, height(),
										col);

	painter.drawRect( sx, 0,
										ex, height());

	painter.drawText((int)sx, 0, ex, height(), AlignCenter, clip->name());
}
