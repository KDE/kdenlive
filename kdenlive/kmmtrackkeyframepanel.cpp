/***************************************************************************
                          kmmtrackkeyframepanel.cpp  -  description
                             -------------------
    begin                : Sun Dec 1 2002
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

#include "kmmtrackkeyframepanel.h"
#include "kdenlivedoc.h"
#include "kmmtimeline.h"
#include "kresizecommand.h"
#include "kdebug.h"

#include "trackpanelclipresizefunction.h"
#include "trackpanelclipmovefunction.h"
#include "trackpanelrazorfunction.h"
#include "trackpanelspacerfunction.h"

KMMTrackKeyFramePanel::KMMTrackKeyFramePanel(KMMTimeLine *timeline,
                                            DocTrackBase *doc,
                                            QWidget *parent,
                                            const char *name ) : 
					KMMTrackPanel(timeline, doc, parent,name)
{
	setMinimumHeight(30);
	setMaximumHeight(30);

	TrackPanelClipResizeFunction *resize = new TrackPanelClipResizeFunction(timeline, doc);
	addFunctionDecorator(KdenliveApp::Move, resize);
	addFunctionDecorator(KdenliveApp::Move, new TrackPanelClipMoveFunction(timeline, doc));

	connect(resize, SIGNAL(signalClipCropStartChanged(const GenTime &)),
					this, SIGNAL(signalClipCropStartChanged(const GenTime &)));
	connect(resize, SIGNAL(signalClipCropEndChanged(const GenTime &)),
					this, SIGNAL(signalClipCropEndChanged(const GenTime &)));

	addFunctionDecorator(KdenliveApp::Razor, new TrackPanelRazorFunction(timeline, doc));
	addFunctionDecorator(KdenliveApp::Spacer, new TrackPanelSpacerFunction(timeline, doc, doc->document()));
}

KMMTrackKeyFramePanel::~KMMTrackKeyFramePanel()
{
}

void KMMTrackKeyFramePanel::paintClip(QPainter & painter, DocClipBase * clip, QRect & rect, bool selected)
{
	int clipsx = (int)timeLine()->mapValueToLocal(clip->trackStart().frames(m_docTrack->document()->framesPerSecond()));
	int clipex = (int)timeLine()->mapValueToLocal(clip->trackEnd().frames(m_docTrack->document()->framesPerSecond()));
	int clipWidth = clipex-clipsx;

  int sx = clipsx;
  int width = clipex;

  if(sx < rect.x()) sx = rect.x();
  if(width > rect.x() + rect.width()) width = rect.x() + rect.width();
  width -= sx;

	painter.setClipping(true);
	painter.setClipRect(sx, rect.y(), width, rect.height());

	QColor col = selected ? QColor(128, 128, 128) : QColor(200, 200, 200);

	painter.fillRect( clipsx, rect.y(),
										clipWidth, rect.height(),
										col);
	painter.drawRect( clipsx, rect.y(),
										clipWidth, rect.height());

  painter.setClipping(false);    
}
