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
#include "kresizecommand.h"
#include "kdebug.h"
#include "ktrackplacer.h"

#include "trackpanelclipresizefunction.h"
#include "trackpanelclipmovefunction.h"
#include "trackpanelrazorfunction.h"
#include "trackpanelspacerfunction.h"

#include "ktrackview.h"
#include "trackpanelfunctionfactory.h"

#include "trackviewbackgrounddecorator.h"
#include "trackviewdoublekeyframedecorator.h"

KMMTrackKeyFramePanel::KMMTrackKeyFramePanel(KTimeLine *timeline,
						KdenliveDoc *doc,
						DocTrackBase *docTrack,
						const QString &effectName,
						int effectIndex,
						const QString &effectParam,
						QWidget *parent,
						const char *name) :
					KMMTrackPanel(timeline, doc, new KTrackPlacer(doc, timeline, docTrack), parent,name)
{
	setMinimumHeight(30);
	setMaximumHeight(30);

	addFunctionDecorator("move", "resize");
	addFunctionDecorator("move", "move");
	addFunctionDecorator("move", "selectnone");
	addFunctionDecorator("razor", "razor");
	addFunctionDecorator("spacer", "spacer");
	addFunctionDecorator("marker", "marker");

	addViewDecorator(new TrackViewBackgroundDecorator(timeline, doc, QColor(128, 128, 128), QColor(200, 200, 200)));
	addViewDecorator(new TrackViewDoubleKeyFrameDecorator(timeline, doc, effectName, effectIndex, effectParam));
}


KMMTrackKeyFramePanel::~KMMTrackKeyFramePanel()
{
}
