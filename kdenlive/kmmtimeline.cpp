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
#include <krulertimemodel.h>

KMMTimeLine::KMMTimeLine(KdenliveDoc *document, QWidget *parent, const char *name ) : QVBox(parent, name),
				m_rulerBox(this, "ruler box"),
				m_trackScroll(this, "track view", WPaintClever),
				m_scrollBox(this, "scroll box"),
				m_trackLabel(i18n("tracks"), &m_rulerBox),
				m_ruler(new KRulerTimeModel(), &m_rulerBox, name),
				m_addTrackButton(i18n("Add Track"), &m_scrollBox),
				m_deleteTrackButton(i18n("Delete Track"), &m_scrollBox),
				m_scrollBar(-100, 5000, 50, 500, 0, QScrollBar::Horizontal, &m_scrollBox, "horizontal ScrollBar")
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

	m_ruler.setValueScale(1.0);
	m_ruler.setRange(0, 3000);
		
	connect(&m_scrollBar, SIGNAL(valueChanged(int)), &m_ruler, SLOT(setStartPixel(int)));
  connect(m_document, SIGNAL(trackListChanged()), this, SLOT(syncWithDocument()));	
  	
  setAcceptDrops(true);

	syncWithDocument();
}

KMMTimeLine::~KMMTimeLine()
{
}

void KMMTimeLine::appendTrack(KMMTrackPanel *track)
{	
	insertTrack(m_trackList.count(), track);
}

/** Inserts a track at the position specified by index */
void KMMTimeLine::insertTrack(int index, KMMTrackPanel *track)
{
	track->reparent(m_trackScroll.viewport(), 0, QPoint(0, 0), TRUE);
	m_trackScroll.addChild(track);
	
	m_trackList.insert(index, track);

	connect(&m_scrollBar, SIGNAL(valueChanged(int)), track, SLOT(drawViewBackBuffer()));
		
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
	
	QWidget *panel = m_trackList.first();
		
	while(panel != 0) {
	  widgetHeight = panel->height();
			
		m_trackScroll.moveChild(panel, 0, height);
		panel->resize(200, widgetHeight);
		
		height+=widgetHeight;
		
		panel = m_trackList.next();
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
		for(count=index; count<m_trackList.count(); count++) {			
			if(&(((KMMTrackPanel *)m_trackList.at(count))->docTrack()) == track) {
				m_trackList.insert(index, m_trackList.take(count));
				break;
			}					
		}
		
		if(count >= m_trackList.count()) {		
			if(track->clipType() == "Video") {
				insertTrack(index, new KMMTrackVideoPanel(*this, *((DocTrackVideo *)track)));
			} else if(track->clipType() == "Sound") {
				insertTrack(index, new KMMTrackSoundPanel(*this, *((DocTrackSound *)track)));
			}
		}
					
		track = m_document->nextTrack();
		index++;		
	}
	
	while(m_trackList.count() > index) {
		m_trackList.remove(index);
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

/** This method maps a local coordinate value to the corresponding
value that should be represented at that position. By using this, there is no need to calculate scale factors yourself. Takes the x
coordinate, and returns the value associated with it. */
double KMMTimeLine::mapLocalToValue(const double coordinate) const
{
	return m_ruler.mapLocalToValue(coordinate);
}

/** Takes the value that we wish to find the coordinate for, and returns the x coordinate. In cases where a single value covers multiple
pixels, the left-most pixel is returned. */
double KMMTimeLine::mapValueToLocal(const double value) const
{
	return m_ruler.mapValueToLocal(value);
}

/** Deselects all clips on the timeline. */
void KMMTimeLine::selectNone()
{
	QPtrListIterator<KMMTrackPanel> itt(m_trackList);

	for(KMMTrackPanel * track; (track=itt.current()) != 0; ++itt) {
		track->selectNone();
	}
}

/** This event occurs when a mouse button is pressed. */
void KMMTimeLine::mousePressEvent(QMouseEvent *event)
{
/*	if(event->button() == LeftButton) {
		if(event->state() & ControlButton) {
			docTrack().toggleSelectClipAt( (int)getTimeLine().mapLocalToValue(event->x()));
		} else if(event->state() & ShiftButton) {
			docTrack().selectClipAt( (int)getTimeLine().mapLocalToValue(event->x()));
		} else {
			getTimeLine().selectNone();
			docTrack().selectClipAt( (int)getTimeLine().mapLocalToValue(event->x()));
		}
	}

	drawToBackBuffer();*/
}

/** This event occurs when a mouse button is released. */
void KMMTimeLine::mouseReleaseEvent(QMouseEvent *event)
{
}

/** This event occurs when the mouse has been moved. */
void KMMTimeLine::mouseMoveEvent(QMouseEvent *event)
{
}
