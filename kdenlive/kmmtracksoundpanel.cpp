/***************************************************************************
                          kmmtracksoundpanel.cpp  -  description
                             -------------------
    begin                : Tue Apr 9 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/
/*decorators added by MArco Gittler (g.marco@freenet.de)*/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qsizepolicy.h>
#include <qtoolbutton.h>

#include <kstandarddirs.h>
#include <kiconloader.h>

#include "kmmtracksoundpanel.h"
#include "ktrackplacer.h"
#include "klocale.h"

#include "kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "trackviewbackgrounddecorator.h"
#include "trackviewmarkerdecorator.h"
#include "trackviewaudiobackgrounddecorator.h"
#include "trackviewdoublekeyframedecorator.h"
#include "trackviewnamedecorator.h"
#include "flatbutton.h"

namespace Gui {

    KMMTrackSoundPanel::KMMTrackSoundPanel(KdenliveApp *,
	KTimeLine * timeline,
	KdenliveDoc * document,
	DocTrackSound * docTrack,
	bool isCollapsed,
	QWidget * parent,
	const char *name):KMMTrackPanel(timeline, document,
	new KTrackPlacer(document, timeline, docTrack), SOUNDTRACK, parent,
        name), m_trackHeader(this, "Sound Track") {

            FlatButton *fl = new FlatButton(m_trackHeader.container, "expand", KGlobal::iconLoader()->loadIcon("kdenlive_down",KIcon::Toolbar,16), KGlobal::iconLoader()->loadIcon("kdenlive_right",KIcon::Toolbar,16), false);

	    m_trackHeader.trackNumber->setText(i18n("Track %1").arg(document->trackIndex(docTrack)));

	    m_mute = docTrack->isMute();
            FlatButton *fl3 = new FlatButton(m_trackHeader.container_3, "audio", KGlobal::iconLoader()->loadIcon("kdenlive_audiooff",KIcon::Toolbar,16), KGlobal::iconLoader()->loadIcon("kdenlive_audioon",KIcon::Toolbar,16), m_mute);

	trackIsCollapsed = isCollapsed;
        connect(fl, SIGNAL(clicked()), this, SLOT(resizeTrack()));
        connect(fl3, SIGNAL(clicked()), this, SLOT(muteTrack()));

	addFunctionDecorator("move", "keyframe");
	addFunctionDecorator("move", "resize");
	addFunctionDecorator("move", "move");
	addFunctionDecorator("move", "selectnone");
	addFunctionDecorator("razor", "razor");
	addFunctionDecorator("spacer", "spacer");
	addFunctionDecorator("marker", "marker");
	addFunctionDecorator("roll", "roll");
	addFunctionDecorator("select", "move");
	addFunctionDecorator("select", "selectnone");

	decorateTrack();

    } 
    
    KMMTrackSoundPanel::~KMMTrackSoundPanel() {
    }

    // virtual
    void KMMTrackSoundPanel::setSelected(bool isSelected)
    {
	if (isSelected) m_trackHeader.setPaletteBackgroundColor(KdenliveSettings::audioclipcolor());
	else m_trackHeader.setPaletteBackgroundColor(QColor(170, 170, 170));
	repaint();
    }

    void KMMTrackSoundPanel::muteTrack()
    {
        m_mute = !m_mute;
        document()->track(documentTrackIndex())->mute(m_mute);
        document()->activateSceneListGeneration(true);
    }
    
    
    void KMMTrackSoundPanel::resizeTrack() {
	trackIsCollapsed = (!trackIsCollapsed);

	clearViewDecorators();
	decorateTrack();
	emit collapseTrack(this, trackIsCollapsed);
    }

    void KMMTrackSoundPanel::decorateTrack() {
	uint widgetHeight;

	if (trackIsCollapsed)
	    widgetHeight = collapsedTrackSize;
	else
	    widgetHeight = KdenliveSettings::audiotracksize();

	setMinimumHeight(widgetHeight);
	setMaximumHeight(widgetHeight);

	if (KdenliveSettings::audiothumbnails() && !trackIsCollapsed)
	    addViewDecorator(new
		TrackViewAudioBackgroundDecorator(timeline(), document(),
		    KdenliveSettings::audioclipcolor(), KdenliveSettings::limitedaudiothumbs()));

	else
	    addViewDecorator(new TrackViewBackgroundDecorator(timeline(),
		    document(), KdenliveSettings::audioclipcolor()));

	addViewDecorator(new TrackViewNameDecorator(timeline(),
		document()));
	addViewDecorator(new TrackViewMarkerDecorator(timeline(),
		document(), this));
	if (!trackIsCollapsed) addViewDecorator(new TrackViewDoubleKeyFrameDecorator(timeline(), document()));

/*	if (trackIsCollapsed)
	    m_trackHeader.collapseButton->setPixmap(KGlobal::iconLoader()->
		loadIcon("1downarrow", KIcon::Small, 16));
	else
	    m_trackHeader.collapseButton->setPixmap(KGlobal::iconLoader()->
        loadIcon("1rightarrow", KIcon::Small, 16));*/
    }

/*
void KMMTrackSoundPanel::paintClip(QPainter & painter, DocClipRef * clip, QRect &rect, bool selected)
{
	int sx = (int)timeLine()->mapValueToLocal(clip->trackStart().frames(25));
	int ex = (int)timeLine()->mapValueToLocal(clip->trackStart().frames(25) + clip->cropDuration().frames(25));

	if(sx < rect.x()) {
		sx = rect.x();
	}
	if(ex > rect.x() + rect.width()) {
		ex = rect.x() + rect.width();
	}
	ex -= sx;

	QColor col = selected ? QColor(64, 128, 64) : QColor(128, 255, 128);

	painter.fillRect( sx, rect.y(),
										ex, rect.height(),
										col);
	painter.drawRect( sx, rect.y(),
										ex, rect.height());
}
*/

}				// namespace Gui
