/***************************************************************************
                          kmmtrackpanel.cpp  -  description
                             -------------------
    begin                : Tue Aug 6 2002
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

#include "kmmtrackpanel.h"
#include "kmmtimeline.h"
#include "kdenlivedoc.h"

#include <kdebug.h>
#include <iostream>

int KMMTrackPanel::resizeTolerance = 4;

KMMTrackPanel::KMMTrackPanel(KMMTimeLine *timeline, DocTrackBase * docTrack, QWidget *parent, const char *name) :
					        			QHBox(parent,name),
												m_docTrack(docTrack),
												m_timeline(timeline)
{
  setMinimumWidth(200);
  setMaximumWidth(200);
  setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding));
  setPalette( QPalette( QColor(170, 170, 170) ) );

  setFrameStyle(QFrame::Panel | QFrame::Sunken);
}

KMMTrackPanel::~KMMTrackPanel()
{
}

/** Read property of KMMTimeLine * m_timeline. */
KMMTimeLine * KMMTrackPanel::timeLine()
{
	return m_timeline;
}

/** returns the document track which is displayed by this track */
DocTrackBase * KMMTrackPanel::docTrack()
{
	return m_docTrack;
}

/** Draws the trackto a back buffered QImage, but does not display it. This image can then be
blitted straight to the screen for speedy drawing. */
void KMMTrackPanel::drawToBackBuffer(QPainter &painter, QRect &rect)
{
	GenTime startValue = GenTime(timeLine()->mapLocalToValue(0.0), m_docTrack->document()->framesPerSecond());
	GenTime endValue = GenTime(timeLine()->mapLocalToValue(rect.width()), m_docTrack->document()->framesPerSecond());
	
	QPtrListIterator<DocClipBase> clip = docTrack()->firstClip(startValue, endValue, false);
	DocClipBase *endClip = docTrack()->endClip(startValue, endValue, false).current();		
	for(DocClipBase *curClip; (curClip = clip.current())!=endClip; ++clip) {
		paintClip(painter, curClip, rect, false);
	}

	clip = docTrack()->firstClip(startValue, endValue, true);
	endClip = docTrack()->endClip(startValue, endValue, true).current();
	for(DocClipBase *curClip; (curClip = clip.current())!=endClip; ++clip) {
		paintClip(painter, curClip, rect, true);
	}

	// draw the vertical time marker

	int value = (int)timeLine()->mapValueToLocal(timeLine()->seekPosition().frames(m_docTrack->document()->framesPerSecond()));
	if(value >= rect.x() && value <= rect.x()+rect.width()) {
		painter.drawLine(value, rect.y(), value, rect.y() + rect.height());
	}
}
