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
#include <kdebug.h>
#include <klocale.h>
#include <qtoolbutton.h>
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

namespace Gui
{

KMMTrackVideoPanel::KMMTrackVideoPanel(KdenliveApp *app,
					KTimeLine *timeline,
					KdenliveDoc *doc,
					DocTrackVideo *docTrack,
					bool isCollapsed, 
					QWidget *parent,
				       	const char *name ) :
		KMMTrackPanel(timeline, doc, new KTrackPlacer(doc, timeline, docTrack), VIDEOTRACK, parent,name),
		m_trackHeader( this, "video header")
{
	m_trackHeader.trackLabel->setText(i18n("Video Track"));
	m_trackIsCollapsed = isCollapsed;
	connect (m_trackHeader.collapseButton, SIGNAL(clicked()), this, SLOT(resizeTrack()));

	addFunctionDecorator("move", "resize");
	addFunctionDecorator("move", "move");
	addFunctionDecorator("move", "selectnone");
	addFunctionDecorator("razor", "razor");
	addFunctionDecorator("spacer", "spacer");
	addFunctionDecorator("marker", "marker");
	addFunctionDecorator("roll", "roll");

	decorateTrack();
}

KMMTrackVideoPanel::~KMMTrackVideoPanel()
{
}

void KMMTrackVideoPanel::resizeTrack()
{
	m_trackIsCollapsed = (!m_trackIsCollapsed);
	clearViewDecorators();
	decorateTrack();
	emit collapseTrack(this, m_trackIsCollapsed);
}

void KMMTrackVideoPanel::decorateTrack()
{
	uint widgetHeight;

	if (m_trackIsCollapsed) widgetHeight = collapsedTrackSize;
	else widgetHeight = KdenliveSettings::videotracksize();

	setMinimumHeight(widgetHeight);
	setMaximumHeight(widgetHeight);
	
	// Show video thumbnails if user
	if (KdenliveSettings::videothumbnails() && !m_trackIsCollapsed)
	addViewDecorator(new TrackViewVideoBackgroundDecorator(timeline(), document(), KdenliveSettings::selectedvideoclipcolor(), KdenliveSettings::videoclipcolor(),0));
	else 
	// Color only decoration
	addViewDecorator(new TrackViewBackgroundDecorator(timeline(), document(), KdenliveSettings::selectedvideoclipcolor(), KdenliveSettings::videoclipcolor()));

	/* should be removed... audio decoration should only be on audio tracks */
	//addViewDecorator(new TrackViewAudioBackgroundDecorator(timeline, doc, QColor(64, 128, 64), QColor(128, 255, 128),audioDecoratorSize));

	addViewDecorator(new TrackViewNameDecorator(timeline(), document()));
	addViewDecorator(new TrackViewMarkerDecorator(timeline(), document()));

	if (m_trackIsCollapsed) m_trackHeader.collapseButton->setPixmap(KGlobal::iconLoader()->loadIcon("1downarrow",KIcon::Small,16));
	else m_trackHeader.collapseButton->setPixmap(KGlobal::iconLoader()->loadIcon("1rightarrow",KIcon::Small,16));

}

} // namespace Gui
