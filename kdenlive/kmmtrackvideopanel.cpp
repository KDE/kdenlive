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

#include <qcursor.h>
#include <kdebug.h>
#include <klocale.h>

#include <cmath>

#include "kdenlivedoc.h"
#include "kmmtrackvideopanel.h"
#include "kmmtimeline.h"
#include "kresizecommand.h"

#include "trackpanelclipmovefunction.h"
#include "trackpanelclipresizefunction.h"
#include "trackpanelrazorfunction.h"
#include "trackpanelspacerfunction.h"

KMMTrackVideoPanel::KMMTrackVideoPanel(KMMTimeLine *timeline,
					DocTrackVideo *docTrack,
					QWidget *parent,
				       	const char *name ) :
		KMMTrackPanel(timeline, docTrack, parent,name),
		m_trackLabel(i18n("Video Track"), this, "Video Track")
{
	setMinimumHeight(20);
	setMaximumHeight(20);

	TrackPanelClipResizeFunction *resize = new TrackPanelClipResizeFunction(timeline, docTrack);
	addFunctionDecorator(KdenliveApp::Move, resize);
	addFunctionDecorator(KdenliveApp::Move, new TrackPanelClipMoveFunction(timeline, docTrack));

	connect(resize, SIGNAL(signalClipCropStartChanged(const GenTime &)),
					this, SIGNAL(signalClipCropStartChanged(const GenTime &)));
	connect(resize, SIGNAL(signalClipCropEndChanged(const GenTime &)),
					this, SIGNAL(signalClipCropEndChanged(const GenTime &)));

	addFunctionDecorator(KdenliveApp::Razor, new TrackPanelRazorFunction(timeline, docTrack));
	addFunctionDecorator(KdenliveApp::Spacer, new TrackPanelSpacerFunction(timeline, docTrack, docTrack->document()));

	m_pixmap.load("test.png");
}

KMMTrackVideoPanel::~KMMTrackVideoPanel()
{
}

/** Paint the specified clip on screen within the specified rectangle, using the specified painter. */
void KMMTrackVideoPanel::paintClip(QPainter &painter, DocClipBase *clip, QRect &rect, bool selected)
{
	int sx = (int)timeLine()->mapValueToLocal(clip->trackStart().frames(m_docTrack->document()->framesPerSecond()));
	int ex = (int)timeLine()->mapValueToLocal(clip->trackEnd().frames(m_docTrack->document()->framesPerSecond()));
	int clipWidth = ex-sx;
	int tx = ex;

	if(sx < rect.x()) {
		sx = rect.x();
	}
	if(ex > rect.x() + rect.width()) {
		ex = rect.x() + rect.width();
	}
	ex -= sx;

	QColor col = selected ? QColor(128, 64, 64) : QColor(255, 128, 128);

	// draw outline box
	painter.fillRect( sx, rect.y(), ex, rect.height(), col);

//	for(uint mycount = sx; mycount < sx+ex; mycount += m_pixmap.width())
//	{
//		painter.drawPixmap(mycount, rect.y(), m_pixmap, 0, 0, -1, rect.height());
//	}
	
	painter.drawRect( sx, rect.y(), ex, rect.height());

	painter.setClipping(true);
	painter.setClipRect(sx, rect.y(), ex, rect.height());
	// draw video name text
	QRect textBound = painter.boundingRect(0, 0, ex, rect.height(), AlignLeft, clip->name());

	double border = 50.0;
	int nameRepeat = (int)std::floor((double)clipWidth / ((double)textBound.width() + border));
	if(nameRepeat < 1) nameRepeat = 1;
	int textWidth = clipWidth / nameRepeat;

	if(textWidth > 0) {
		int start = (int)timeLine()->mapValueToLocal(clip->trackStart().frames(25));

		start = sx - ((sx - start) % textWidth);
		int count = start;

		while(count < sx+ex) {
			if(count+textWidth <= tx) {
        		painter.setPen( selected ? Qt::white : Qt::black );
				painter.drawText( count, rect.y(), textWidth, rect.height(), AlignVCenter | AlignHCenter, clip->name());
        		painter.setPen(Qt::black);
			}
			count += textWidth;
		}
	}

	painter.setClipping(false);
}
