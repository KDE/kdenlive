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


namespace Gui {

    KMMTrackKeyFramePanel::KMMTrackKeyFramePanel(KTimeLine * timeline,
	KdenliveDoc * doc,
	DocTrackBase * docTrack,
	bool isCollapsed,
	const QString & effectName,
	int effectIndex,
	const QString & effectParam,
	TRACKTYPE type,
	QWidget * parent,
	const char *name):KMMTrackPanel(timeline, doc,
	new KTrackPlacer(doc, timeline, docTrack), type, parent, name) {
	m_type = type;
	m_trackIsCollapsed = isCollapsed;
	uint widgetHeight;
	if (type == EFFECTKEYFRAMETRACK)
	    widgetHeight = 50;
	else
	     widgetHeight = 30;
	if (m_trackIsCollapsed)
	     widgetHeight = 0;

	 setMinimumHeight(widgetHeight);
	 setMaximumHeight(widgetHeight);

	 addFunctionDecorator("move", "keyframe");
	//addFunctionDecorator("move", "move");
	//addFunctionDecorator("move", "selectnone");
	//addFunctionDecorator("razor", "razor");
	 addFunctionDecorator("spacer", "spacer");
	 addFunctionDecorator("marker", "marker");



	//addViewDecorator(new TrackViewBackgroundDecorator(timeline, doc, QColor(128, 128, 128), QColor(200, 200, 200)));
	 addViewDecorator(new TrackViewDoubleKeyFrameDecorator(timeline,
		doc, effectName, effectIndex, effectParam));
        }
    
    void KMMTrackKeyFramePanel::resizeTrack() {
	//clearViewDecorators();
	uint widgetHeight;
	if (m_type == EFFECTKEYFRAMETRACK)
	    widgetHeight = 50;
	else
	    widgetHeight = 30;
	m_trackIsCollapsed = (!m_trackIsCollapsed);

	if (m_trackIsCollapsed)
	    widgetHeight = 0;
	setMinimumHeight(widgetHeight);
	setMaximumHeight(widgetHeight);
//      addViewDecorator(new TrackViewBackgroundDecorator(timeline(), document(), QColor(128, 128, 128), QColor(200, 200, 200)));
//      addViewDecorator(new TrackViewDoubleKeyFrameDecorator(timeline(), document(), effectName, effectIndex, effectParam));
    }

    KMMTrackKeyFramePanel::~KMMTrackKeyFramePanel() {
    }

}				// namespace Gui
