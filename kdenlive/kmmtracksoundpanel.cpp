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

KMMTrackSoundPanel::KMMTrackSoundPanel(KMMTimeLine & timeline, DocTrackSound & docTrack, QWidget *parent, const char *name ) :
												KMMTrackPanel(timeline, docTrack, parent,name),
												m_trackLabel(this, "Sound Track")
{
	setMinimumWidth(200);
	setMaximumWidth(200);

	setMinimumHeight(30);
	setMaximumHeight(30);	
	setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding));
}

KMMTrackSoundPanel::~KMMTrackSoundPanel()
{
}

/** This function will paint a clip on screen, using the specified painter and the given coordinates as to
	where the clip should be painted. */
void KMMTrackSoundPanel::paintClip(QPainter & painter, DocClipBase * clip, QRect &rect, bool selected)
{
	int sx = (int)timeLine().mapValueToLocal(clip->trackStart().frames(25));
	int ex = (int)timeLine().mapValueToLocal(clip->trackStart().frames(25) + clip->cropDuration().frames(25));

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
/** No descriptions */
bool KMMTrackSoundPanel::mouseMoved(QMouseEvent *event)
{
	return true;
}

/** No descriptions */
bool KMMTrackSoundPanel::mousePressed(QMouseEvent *event)
{
	return true;
}

/** No descriptions */
bool KMMTrackSoundPanel::mouseReleased(QMouseEvent *event)
{
	return true;
}

/** No descriptions */
QCursor KMMTrackSoundPanel::getMouseCursor(QMouseEvent *event)
{
}
