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
#include <qscrollbar.h>
#include <qscrollview.h>
#include <qhbox.h>

#include <kdebug.h>

#include "kdenlivedoc.h"
#include "kmmtimeline.h"
#include "kmmtracksoundpanel.h"
#include "kmmtrackvideopanel.h"
#include "kmmtimelinetrackview.h"
#include "krulertimemodel.h"
#include "kscalableruler.h"
#include "clipdrag.h"

KMMTimeLine::KMMTimeLine(QWidget *rulerToolWidget, QWidget *scrollToolWidget, KdenliveDoc *document, QWidget *parent, const char *name ) :
				QVBox(parent, name),
				m_document(document)				
{
	m_rulerBox = new QHBox(this, "ruler box");	
	m_trackScroll = new QScrollView(this, "track view", WPaintClever);
	m_scrollBox = new QHBox(this, "scroll box");

	m_rulerToolWidget = rulerToolWidget;
	if(!m_rulerToolWidget) m_rulerToolWidget = new QLabel("Tracks", 0, "Tracks");	
	m_rulerToolWidget->reparent(m_rulerBox, QPoint(0,0));
	m_ruler = new KScalableRuler(new KRulerTimeModel(), m_rulerBox, name);

	m_scrollToolWidget = scrollToolWidget;
	if(!m_scrollToolWidget) m_scrollToolWidget = new QLabel("Scroll", 0, "Scroll");	
	m_scrollToolWidget->reparent(m_scrollBox, QPoint(0,0));	
	m_scrollBar = new QScrollBar(-100, 5000, 50, 500, 0, QScrollBar::Horizontal, m_scrollBox, "horizontal ScrollBar");
	
	m_trackViewArea = new KMMTimeLineTrackView(*this, m_trackScroll, "track view area");
	
	m_trackScroll->enableClipper(TRUE);
	m_trackScroll->setVScrollBarMode(QScrollView::AlwaysOn);
	m_trackScroll->setHScrollBarMode(QScrollView::AlwaysOff);

	m_rulerToolWidget->setMinimumWidth(200);
	m_rulerToolWidget->setMaximumWidth(200);

	m_scrollToolWidget->setMinimumWidth(200);
	m_scrollToolWidget->setMaximumWidth(200);
		
	m_ruler->setValueScale(1.0);
	m_ruler->setRange(0, 3000);
		
	connect(m_scrollBar, SIGNAL(valueChanged(int)), m_ruler, SLOT(setStartPixel(int)));
	connect(m_scrollBar, SIGNAL(valueChanged(int)), m_ruler, SLOT(repaint()));	
  connect(m_document, SIGNAL(trackListChanged()), this, SLOT(syncWithDocument()));	
  	
  setAcceptDrops(true);

	syncWithDocument();

	m_startedClipMove = false;	
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
	track->reparent(m_trackScroll->viewport(), 0, QPoint(0, 0), TRUE);
	m_trackScroll->addChild(track);
	
	m_trackList.insert(index, track);

	connect(m_scrollBar, SIGNAL(valueChanged(int)), this, SLOT(drawTrackViewBackBuffer()));
	connect(&track->docTrack(), SIGNAL(trackChanged()), this, SLOT(drawTrackViewBackBuffer()));
		
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
			
		m_trackScroll->moveChild(panel, 0, height);
		panel->resize(200, widgetHeight);
		
		height+=widgetHeight;
		
		panel = m_trackList.next();
	}

	m_trackScroll->moveChild(m_trackViewArea, 200, 0);
	m_trackViewArea->resize(m_trackScroll->visibleWidth()-200 , height);

  int viewWidth = m_trackScroll->visibleWidth()-200;
  if(viewWidth<1) viewWidth=1;
	
	QPixmap pixmap(viewWidth , height);
	
	m_trackScroll->resizeContents(m_trackScroll->visibleWidth(), height);	
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
	QPoint mouse = m_trackViewArea->mapFrom(this, event->pos());

	if(m_startedClipMove) {
		event->accept(true);
	} else 	if(ClipDrag::canDecode(event)) {
		kdWarning() << "Decoding..." << endl;
		m_selection = ClipDrag::decode(*m_document, event);
		m_clipOffset = 0;

		if(m_selection.isEmpty()) {
			event->accept(false);
		} else {
			int mouseX = (int)mapLocalToValue(mouse.x());
      int temp = 0;
         			
			if(m_selection.findClosestMatchingSpace(mouseX, temp)) {
				addGroupToTracks(m_selection, trackUnderPoint(mouse), mouseX);
				event->accept(true);
			} else {
				event->accept(false);
			}
		}
	} else {
		event->accept(false);
	}

	m_startedClipMove = false;
}

void KMMTimeLine::dragMoveEvent ( QDragMoveEvent *event )
{
	kdWarning() << "DragMoveEvent" << endl;
	QPoint pos = m_trackViewArea->mapFrom(this, event->pos());
	moveSelectedClips(trackUnderPoint(pos), (int)(mapLocalToValue(pos.x()) - m_clipOffset));

/*			if(event->x() < 0) {
			m_timeLine.scrollViewLeft();
		} else if(event->x() > width()) {
			m_timeLine.scrollViewRight();
		}*/
}

void KMMTimeLine::dragLeaveEvent ( QDragLeaveEvent *event )
{
	kdWarning() << "DragLeaveEvent" << endl;
}

void KMMTimeLine::dropEvent ( QDropEvent *event )
{
	kdWarning() << "DragDropEvent" << endl;
}

/** This method maps a local coordinate value to the corresponding
value that should be represented at that position. By using this, there is no need to calculate scale factors yourself. Takes the x
coordinate, and returns the value associated with it. */
double KMMTimeLine::mapLocalToValue(const double coordinate) const
{
	return m_ruler->mapLocalToValue(coordinate);
}

/** Takes the value that we wish to find the coordinate for, and returns the x coordinate. In cases where a single value covers multiple
pixels, the left-most pixel is returned. */
double KMMTimeLine::mapValueToLocal(const double value) const
{
	return m_ruler->mapValueToLocal(value);
}

/** Deselects all clips on the timeline. */
void KMMTimeLine::selectNone()
{
	m_selection.removeAllClips();
}

void KMMTimeLine::drawTrackViewBackBuffer()
{
	m_trackViewArea->drawBackBuffer();
}

/** Returns m_trackList

Warning - this method is a bit of a hack, not good OO practice, and should be removed at some point. */
QPtrList<KMMTrackPanel> &KMMTimeLine::trackList()
{
	return m_trackList;
}

/** Moves all selected clips to a new position. The new start position is that for the master clip, all other clips are moved in relation to it. */
void KMMTimeLine::moveSelectedClips(int track, int start)
{
	m_selection.moveTo(*m_document, track, start);
	
	drawTrackViewBackBuffer();
}

/** Scrolls the track view area left by whatever the step value of the relevant scroll bar is. */
void KMMTimeLine::scrollViewLeft()
{
	m_scrollBar->subtractLine();
}

/** Scrolls the track view area right by whatever the step value in the 
relevant scrollbar is. */
void KMMTimeLine::scrollViewRight()
{
	m_scrollBar->addLine();
}

/** Toggle Selects the clip on the given track and at the given value. The clip will become selected if it wasn't already selected, and will be deselected if it is. */
void KMMTimeLine::toggleSelectClipAt(DocTrackBase &track, int value)
{
	m_selection.toggleClip(track.getClipAt(value));
}

/** Selects the clip on the given track at the given value. */
void KMMTimeLine::selectClipAt(DocTrackBase &track, int value)
{
	m_selection.addClip(track.getClipAt(value));
}

/** Returns true if the clip is selected, false otherwise. */
bool KMMTimeLine::clipSelected(DocClipBase *clip)
{
	return m_selection.clipExists(clip);
}

/** Adds a Clipgroup to the tracks in the timeline. */
void KMMTimeLine::addGroupToTracks(ClipGroup &group, int track, int value)
{
	group.moveTo(*m_document, track, value);
}

/** Returns the integer value of the track underneath the mouse cursor. 
The track number is that in the track list of the document, which is
sorted incrementally from top to bottom. i.e. track 0 is topmost, track 1 is next down, etc. */
int KMMTimeLine::trackUnderPoint(const QPoint &pos)
{
	QPtrListIterator<KMMTrackPanel> itt(m_trackList);
	int height=0;
	int widgetHeight;
	int count;
	KMMTrackPanel *panel;

	for(count=0; (panel = itt.current()); ++itt, ++count) {
  	widgetHeight = panel->height();

   	if((pos.y() > height) && (pos.y() <= height + widgetHeight)) {
    	return count;
    }

    height += widgetHeight;
	}
	
	return -1;
}

/** Initiates a drag operation on the selected clip, setting the master clip to clipUnderMouse, and
 the x offset to clipOffset. */
void KMMTimeLine::initiateDrag(DocClipBase *clipUnderMouse, double clipOffset)
{
	kdWarning() << "Initiating drag" << endl;
	m_selection.setMasterClip(clipUnderMouse);
	m_clipOffset = clipOffset;
	m_startedClipMove = true;

	ClipDrag *clip = new ClipDrag(m_selection, this, "Timeline Drag");
	clip->dragCopy();
}
