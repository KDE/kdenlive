/***************************************************************************
                          kmmtimeline.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 2002
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

#include "kmmtimeline.h"
#include "kmmtracksoundpanel.h"
#include "kmmtrackvideopanel.h"
#include "kmmtrackvideo.h"
#include "kmmtracksound.h"

KMMTimeLine::KMMTimeLine(QWidget *parent, const char *name ) : QVBox(parent, name),
				m_rulerBox(this, "ruler box"),
				m_trackScroll(this, "track view", WPaintClever),
				m_scrollBox(this, "scroll box"),
				m_trackLabel("tracks", &m_rulerBox),
				m_ruler(&m_rulerBox, name),
				m_scrollLabel("tracks", &m_scrollBox),
				m_scrollBar(0, 5000, 50, 500, 0, QScrollBar::Horizontal, &m_scrollBox, "horizontal ScrollBar")
{	
	m_trackScroll.enableClipper(TRUE);
	m_trackScroll.setVScrollBarMode(QScrollView::AlwaysOn);
	m_trackScroll.setHScrollBarMode(QScrollView::AlwaysOff);

	m_trackLabel.setMinimumWidth(200);
	m_trackLabel.setMaximumWidth(200);
	m_trackLabel.setAlignment(AlignCenter);
	
	m_scrollLabel.setMinimumWidth(200);
	m_scrollLabel.setMaximumWidth(200);
	m_scrollLabel.setAlignment(AlignCenter);
	
	appendTrack(new KMMTrackVideoPanel(), new KMMTrackVideo());
	appendTrack(new KMMTrackVideoPanel(), new KMMTrackVideo());
	appendTrack(new KMMTrackVideoPanel(), new KMMTrackVideo());		
	appendTrack(new KMMTrackSoundPanel(), new KMMTrackSound());
	appendTrack(new KMMTrackSoundPanel(), new KMMTrackSound());	
	appendTrack(new KMMTrackSoundPanel(), new KMMTrackSound());		
	
	connect(&m_scrollBar, SIGNAL(valueChanged(int)), &m_ruler, SLOT(setValue(int)));
}

KMMTimeLine::~KMMTimeLine()
{
}

void KMMTimeLine::appendTrack(QWidget *trackPanel, QWidget *trackView)
{	
	trackPanel->reparent(m_trackScroll.viewport(), 0, QPoint(0, 0), TRUE);
	m_trackScroll.addChild(trackPanel);
	trackView->reparent(m_trackScroll.viewport(), 0, QPoint(0, 0), TRUE);
	m_trackScroll.addChild(trackView);
	
	m_trackPanels.append(trackPanel);
	m_trackViews.append(trackView);
		
	resizeTracks();	
}

void KMMTimeLine::resizeEvent(QResizeEvent *event)
{
	resizeTracks();
}

void KMMTimeLine::resizeTracks()
{
	int height = 0;
	int widgetHeight;
	
	QWidget *panel = m_trackPanels.first();
	QWidget *view = m_trackViews.first();
		
	while(panel != 0) {
//		widgetHeight = (panel->height() > view->height()) ? panel->height() : view->height();
	  widgetHeight = panel->height();
			
		m_trackScroll.moveChild(panel, 0, height);
		panel->resize(200, widgetHeight);
		
		m_trackScroll.moveChild(view, 200, height);
		view->resize(m_trackScroll.visibleWidth() - 200, widgetHeight);
		height+=widgetHeight;
		
		panel = m_trackPanels.next();
		view = m_trackViews.next();
	}
	
	m_trackScroll.resizeContents(m_trackScroll.visibleWidth(), height);	
}

