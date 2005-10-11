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
#include "kresizecommand.h"
#include "ktrackplacer.h"

#include "trackpanelclipmovefunction.h"
#include "trackpanelclipresizefunction.h"
#include "trackpanelcliprollfunction.h"
#include "trackpanelrazorfunction.h"
#include "trackpanelspacerfunction.h"
#include "trackpanelmarkerfunction.h"

#include "trackviewbackgrounddecorator.h"
#include "trackviewnamedecorator.h"
#include "trackviewmarkerdecorator.h"

namespace Gui
{

KMMTrackVideoPanel::KMMTrackVideoPanel(KdenliveApp *app,
					KTimeLine *timeline,
					KdenliveDoc *doc,
					DocTrackVideo *docTrack,
					QWidget *parent,
				       	const char *name ) :
		KMMTrackPanel(timeline, doc, new KTrackPlacer(doc, timeline, docTrack), parent,name),
		m_trackLabel(i18n("Video Track"), this, "Video Track")
{
	setMinimumHeight(20);
	setMaximumHeight(20);

	addFunctionDecorator("move", "resize");
	addFunctionDecorator("move", "move");
	addFunctionDecorator("move", "selectnone");
	addFunctionDecorator("razor", "razor");
	addFunctionDecorator("spacer", "spacer");
	addFunctionDecorator("marker", "marker");
	addFunctionDecorator("roll", "roll");

	addViewDecorator(new TrackViewBackgroundDecorator(timeline, doc, QColor(128, 64, 64), QColor(255, 128, 128)));
	addViewDecorator(new TrackViewNameDecorator(timeline, doc));
	addViewDecorator(new TrackViewMarkerDecorator(timeline, doc));
}

KMMTrackVideoPanel::~KMMTrackVideoPanel()
{
}

} // namespace Gui
