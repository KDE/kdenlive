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

#include "trackviewbackgrounddecorator.h"

KMMTrackKeyFramePanel::KMMTrackKeyFramePanel(KMMTimeLine *timeline,
						KdenliveDoc *doc,
						DocTrackBase *docTrack,
						QWidget *parent, 
						const char *name) :
					KMMTrackPanel(timeline, doc, docTrack, parent,name)
{
	setMinimumHeight(30);
	setMaximumHeight(30);

	TrackPanelClipResizeFunction *resize = new TrackPanelClipResizeFunction(timeline, doc, docTrack);
	addFunctionDecorator(KdenliveApp::Move, resize);
	addFunctionDecorator(KdenliveApp::Move, new TrackPanelClipMoveFunction(timeline, doc, docTrack));

	connect(resize, SIGNAL(signalClipCropStartChanged(const GenTime &)),
					this, SIGNAL(signalClipCropStartChanged(const GenTime &)));
	connect(resize, SIGNAL(signalClipCropEndChanged(const GenTime &)),
					this, SIGNAL(signalClipCropEndChanged(const GenTime &)));

	addFunctionDecorator(KdenliveApp::Razor, new TrackPanelRazorFunction(timeline, doc, docTrack));
	addFunctionDecorator(KdenliveApp::Spacer, new TrackPanelSpacerFunction(timeline, docTrack, document()));

	addViewDecorator(new TrackViewBackgroundDecorator(timeline, doc, docTrack, QColor(128, 128, 128), QColor(200, 200, 200)));
}

KMMTrackKeyFramePanel::~KMMTrackKeyFramePanel()
{
}
