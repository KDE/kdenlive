/***************************************************************************
                          kmmtrackvideopanel.cpp  -  description
                             -------------------
    begin                : Tue Apr 9 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/
/*decorators add by Marco Gittler (g.marco@freenet.de) Oct 2005 */
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qcursor.h>
#include <qpopupmenu.h>

#include <kdebug.h>
#include <klocale.h>
#include <qtoolbutton.h>
#include <kstandarddirs.h>
#include <kiconloader.h>

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
#include "kdenlivesettings.h"

#include "trackviewvideobackgrounddecorator.h"
#include "trackviewaudiobackgrounddecorator.h"
#include "trackviewbackgrounddecorator.h"
#include "trackviewnamedecorator.h"
#include "trackviewmarkerdecorator.h"
#include "flatbutton.h"

namespace Gui {

    KMMTrackVideoPanel::KMMTrackVideoPanel(KdenliveApp *,
	KTimeLine * timeline,
	KdenliveDoc * doc,
	DocTrackVideo * docTrack,
	bool isCollapsed,
	QWidget * parent,
	const char *name):KMMTrackPanel(timeline, doc,
	new KTrackPlacer(doc, timeline, docTrack), VIDEOTRACK, parent,
        name), m_trackHeader(this, "Video Track"), m_mute(false), m_blind(false) {
            
            FlatButton *fl = new FlatButton(m_trackHeader.container, "expand", KGlobal::iconLoader()->loadIcon("kdenlive_down",KIcon::Toolbar,22), KGlobal::iconLoader()->loadIcon("kdenlive_right",KIcon::Toolbar,22), false);
	    m_trackHeader.trackNumber->setText(i18n("Track %1").arg(doc->trackIndex(docTrack)));
            FlatButton *fl2 = new FlatButton(m_trackHeader.container_2, "video", KGlobal::iconLoader()->loadIcon("kdenlive_videooff",KIcon::Toolbar,22), KGlobal::iconLoader()->loadIcon("kdenlive_videoon",KIcon::Toolbar,22), false);
            
            FlatButton *fl3 = new FlatButton(m_trackHeader.container_3, "audio", KGlobal::iconLoader()->loadIcon("kdenlive_audiooff",KIcon::Toolbar,22), KGlobal::iconLoader()->loadIcon("kdenlive_audioon",KIcon::Toolbar,22), false);

	trackIsCollapsed = isCollapsed;
        connect(fl, SIGNAL(clicked()), this, SLOT(resizeTrack()));
        connect(fl2, SIGNAL(clicked()), this, SLOT(blindTrack()));
        connect(fl3, SIGNAL(clicked()), this, SLOT(muteTrack()));

	addFunctionDecorator("move", "resize");
	addFunctionDecorator("move", "move");
	addFunctionDecorator("move", "selectnone");
	addFunctionDecorator("razor", "razor");
	addFunctionDecorator("spacer", "spacer");
	addFunctionDecorator("marker", "marker");
	addFunctionDecorator("roll", "roll");

	decorateTrack();
    } 
    
    KMMTrackVideoPanel::~KMMTrackVideoPanel() {
    }

    void KMMTrackVideoPanel::muteTrack()
    {
        m_mute = !m_mute;
        document()->track(documentTrackIndex())->mute(m_mute);
        document()->activateSceneListGeneration(true);
    }
    
    void KMMTrackVideoPanel::blindTrack()
    {
        m_blind = !m_blind;
        document()->track(documentTrackIndex())->blind(m_blind);
        document()->activateSceneListGeneration(true);
    }

    void KMMTrackVideoPanel::resizeTrack() {
	trackIsCollapsed = (!trackIsCollapsed);
	clearViewDecorators();
	decorateTrack();
	emit collapseTrack(this, trackIsCollapsed);
    }

    void KMMTrackVideoPanel::decorateTrack() {
	uint widgetHeight;

	if (trackIsCollapsed)
	    widgetHeight = collapsedTrackSize;
	else
	    widgetHeight = KdenliveSettings::videotracksize();

	setMinimumHeight(widgetHeight);
	setMaximumHeight(widgetHeight);

	// Show video thumbnails if user
	if (KdenliveSettings::videothumbnails() && !trackIsCollapsed)
	    addViewDecorator(new
		TrackViewVideoBackgroundDecorator(timeline(), document(),
		    KdenliveSettings::selectedvideoclipcolor(),
		    KdenliveSettings::videoclipcolor(), KdenliveSettings::audiothumbnails()));
	else
	    // Color only decoration
	    addViewDecorator(new TrackViewBackgroundDecorator(timeline(),
		    document(), KdenliveSettings::selectedvideoclipcolor(),
		    KdenliveSettings::videoclipcolor()));

	/* should be removed... audio decoration should only be on audio tracks */
	if (KdenliveSettings::audiothumbnails() && !trackIsCollapsed)
	    addViewDecorator(new
		TrackViewAudioBackgroundDecorator(timeline(), document(),
		    KdenliveSettings::selectedaudioclipcolor(),
		    KdenliveSettings::audioclipcolor(), true));

	//addViewDecorator(new TrackViewAudioBackgroundDecorator(timeline, doc, QColor(64, 128, 64), QColor(128, 255, 128),audioDecoratorSize));

	addViewDecorator(new TrackViewNameDecorator(timeline(),
		document()));
	addViewDecorator(new TrackViewMarkerDecorator(timeline(),
		document()));

/*	if (trackIsCollapsed)
	    m_trackHeader.collapseButton->setPixmap(KGlobal::iconLoader()->
		loadIcon("1downarrow", KIcon::Small, 16));
	else
	    m_trackHeader.collapseButton->setPixmap(KGlobal::iconLoader()->
        loadIcon("1rightarrow", KIcon::Small, 16));*/

    }

}				// namespace Gui
