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

#include <klocale.h>

#include <kmmtimeline.h>
#include <kmmtracksoundpanel.h>
#include <kmmtrackvideopanel.h>
#include <kmmtrackvideo.h>
#include <kmmtracksound.h>

KMMTimeLine::KMMTimeLine(KdenliveDoc *document, QWidget *parent, const char *name ) : QVBox(parent, name),
				m_rulerBox(this, "ruler box"),
				m_trackScroll(this, "track view", WPaintClever),
				m_scrollBox(this, "scroll box"),
				m_trackLabel(i18n("tracks"), &m_rulerBox),
				m_ruler(&m_rulerBox, name),
				m_addTrackButton(i18n("Add Track"), &m_scrollBox),
				m_deleteTrackButton(i18n("Delete Track"), &m_scrollBox),
				m_scrollBar(0, 5000, 50, 500, 0, QScrollBar::Horizontal, &m_scrollBox, "horizontal ScrollBar")
{	
	m_document = document;
	
	m_trackScroll.enableClipper(TRUE);
	m_trackScroll.setVScrollBarMode(QScrollView::AlwaysOn);
	m_trackScroll.setHScrollBarMode(QScrollView::AlwaysOff);

	m_trackLabel.setMinimumWidth(200);
	m_trackLabel.setMaximumWidth(200);
	m_trackLabel.setAlignment(AlignCenter);
		
	m_addTrackButton.setMinimumWidth(100);	
	m_addTrackButton.setMaximumWidth(100);
	
	m_deleteTrackButton.setMinimumWidth(100);
	m_deleteTrackButton.setMaximumWidth(100);
		
	connect(&m_scrollBar, SIGNAL(valueChanged(int)), &m_ruler, SLOT(setValue(int)));
  connect(m_document, SIGNAL(trackListChanged()), this, SLOT(syncWithDocument()));	
  	
  setAcceptDrops(true);

	syncWithDocument();
}

KMMTimeLine::~KMMTimeLine()
{
}

void KMMTimeLine::appendTrack(QWidget *trackPanel, KMMTrackBase *trackView)
{	
	insertTrack(m_trackPanels.count(), trackPanel, trackView);
}

/** Inserts a track at the position specified by index */
void KMMTimeLine::insertTrack(int index, QWidget *trackPanel, KMMTrackBase *trackView)
{
	trackPanel->reparent(m_trackScroll.viewport(), 0, QPoint(0, 0), TRUE);
	m_trackScroll.addChild(trackPanel);
	trackView->reparent(m_trackScroll.viewport(), 0, QPoint(0, 0), TRUE);
	m_trackScroll.addChild(trackView);
	
	m_trackPanels.insert(index, trackPanel);
	m_trackViews.insert(index, trackView);
		
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

/** At least one track within the project have been added or removed.
	*
	* The timeline needs to be updated to show these changes. */
void KMMTimeLine::syncWithDocument()
{
	unsigned int index = 0;
	unsigned int count;
	
	DocTrackBase *track = m_document->firstTrack();
	
	while(track != 0) {
		for(count=index; count<m_trackViews.count(); count++) {			
			if(((KMMTrackBase *)m_trackViews.at(count))->docTrack() == track) {
				m_trackViews.insert(index, m_trackViews.take(count));
				m_trackPanels.insert(index, m_trackPanels.take(count));
				break;
			}					
		}
		
		if(count >= m_trackViews.count()) {		
			if(track->clipType() == "Video") {
				insertTrack(index, new KMMTrackVideoPanel((DocTrackVideo *)track), new KMMTrackVideo((DocTrackVideo *)track));
			} else if(track->clipType() == "Sound") {
				insertTrack(index, new KMMTrackSoundPanel((DocTrackSound *)track), new KMMTrackSound((DocTrackSound *)track));
			}
		}
					
		track = m_document->nextTrack();
		index++;		
	}
	
	while(m_trackPanels.count() > index) {
		m_trackPanels.remove(index);
		m_trackViews.remove(index);
	}
	
	resizeTracks();
}

/** No descriptions */
void KMMTimeLine::polish()
{
	resizeTracks();
}

void KMMTimeLine::dragEnterEvent ( QDragEnterEvent *event )
{

}

void KMMTimeLine::dragMoveEvent ( QDragMoveEvent *event )
{
}

void KMMTimeLine::dragLeaveEvent ( QDragLeaveEvent *event )
{
}

void KMMTimeLine::dropEvent ( QDropEvent *event )
{
}
