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
#include "trackpanelmarkerfunction.h"

#include "trackviewbackgrounddecorator.h"
#include "trackviewnamedecorator.h"
#include "trackviewmarkerdecorator.h"

KMMTrackVideoPanel::KMMTrackVideoPanel(KMMTimeLine *timeline,
					KdenliveDoc *doc,
					DocTrackVideo *docTrack,
					QWidget *parent,
				       	const char *name ) :
		KMMTrackPanel(timeline, doc, docTrack, parent,name),
		m_trackLabel(i18n("Video Track"), this, "Video Track")
{
	setMinimumHeight(20);
	setMaximumHeight(20);

	TrackPanelClipResizeFunction *resize = new TrackPanelClipResizeFunction(timeline, doc, docTrack);
	addFunctionDecorator(KdenliveApp::Move, resize);
	addFunctionDecorator(KdenliveApp::Move, new TrackPanelClipMoveFunction(timeline, doc, docTrack));

	connect(resize, SIGNAL(signalClipCropStartChanged(DocClipRef *)), this, SIGNAL(signalClipCropStartChanged(DocClipRef *)));
	connect(resize, SIGNAL(signalClipCropEndChanged(DocClipRef *)), this, SIGNAL(signalClipCropEndChanged(DocClipRef *)));

	addFunctionDecorator(KdenliveApp::Razor, new TrackPanelRazorFunction(timeline, doc, docTrack));
	addFunctionDecorator(KdenliveApp::Spacer, new TrackPanelSpacerFunction(timeline, docTrack, document()));
	addFunctionDecorator(KdenliveApp::Marker, new TrackPanelMarkerFunction(timeline, docTrack, document()));

	addViewDecorator(new TrackViewBackgroundDecorator(timeline, doc, docTrack, QColor(128, 64, 64), QColor(255, 128, 128)));
	addViewDecorator(new TrackViewNameDecorator(timeline, doc, docTrack));
	addViewDecorator(new TrackViewMarkerDecorator(timeline, doc, docTrack));
}

KMMTrackVideoPanel::~KMMTrackVideoPanel()
{
}
