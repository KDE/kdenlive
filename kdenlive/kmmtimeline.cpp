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

#include <math.h>

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

int KMMTimeLine::snapTolerance=20;

KMMTimeLine::KMMTimeLine(QWidget *rulerToolWidget, QWidget *scrollToolWidget, KdenliveDoc *document, QWidget *parent, const char *name ) :
				QVBox(parent, name),
				m_document(document),
				m_selection()
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
	calculateProjectSize(m_ruler->valueScale());
		
	connect(m_scrollBar, SIGNAL(valueChanged(int)), m_ruler, SLOT(setStartPixel(int)));
	connect(m_scrollBar, SIGNAL(valueChanged(int)), m_ruler, SLOT(repaint()));	
  connect(m_document, SIGNAL(trackListChanged()), this, SLOT(syncWithDocument()));
  connect(m_ruler, SIGNAL(scaleChanged(double)), this, SLOT(calculateProjectSize(double)));
  	
  setAcceptDrops(true);

	syncWithDocument();

	m_startedClipMove = false;
	m_masterClip = 0;

	m_gridSnapTracker = m_snapToGridList.end();
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
	if(m_startedClipMove) {
		event->accept(true);
	} else 	if(ClipDrag::canDecode(event)) {
		m_selection = ClipDrag::decode(*m_document, event);

    if(m_selection.masterClip()==0) m_selection.setMasterClip(m_selection.first());
    m_masterClip = m_selection.masterClip();

		m_clipOffset = 0;

		if(m_selection.isEmpty()) {
			event->accept(false);
		} else {
			generateSnapToGridList();
      event->accept(true);
		}
	} else {	
		event->accept(false);
	}

	m_startedClipMove = false;
}

void KMMTimeLine::dragMoveEvent ( QDragMoveEvent *event )
{
	QPoint pos = m_trackViewArea->mapFrom(this, event->pos());
	GenTime timeUnderMouse(mapLocalToValue(pos.x()), 25);	

	if(m_gridSnapTracker != m_snapToGridList.end()) {
		QValueListIterator<GenTime> itt = m_gridSnapTracker;
		++itt;	
		while(itt != m_snapToGridList.end()) {
			if (fabs(((*itt) - timeUnderMouse).seconds()) > fabs(((*m_gridSnapTracker) - timeUnderMouse).seconds())) break;
			++m_gridSnapTracker;
			++itt;
		}

		itt = m_gridSnapTracker;
		--itt;
		while(m_gridSnapTracker != m_snapToGridList.begin()) {
			if (fabs(((*itt) - timeUnderMouse).seconds()) > fabs(((*m_gridSnapTracker) - timeUnderMouse).seconds())) break;	
			--m_gridSnapTracker;
			--itt;
		}

		if( abs((int)mapValueToLocal((*m_gridSnapTracker).frames(25)) - pos.x()) < snapTolerance) {
			timeUnderMouse = *m_gridSnapTracker;
		}
	}	
	
	if(m_selection.isEmpty()) {  	     	  
		moveSelectedClips(trackUnderPoint(pos), timeUnderMouse - m_clipOffset);
	} else {
		if(canAddClipsToTracks(m_selection, trackUnderPoint(pos), timeUnderMouse + m_clipOffset)) {
			addClipsToTracks(m_selection, trackUnderPoint(pos), timeUnderMouse + m_clipOffset, true);
			generateSnapToGridList();
			m_selection.clear();
		}
	}
	calculateProjectSize(m_ruler->valueScale());
}

void KMMTimeLine::dragLeaveEvent ( QDragLeaveEvent *event )
{
	// In a drag Leave Event, any clips in the selection are removed from the timeline.

	m_selection.clear();

	QPtrListIterator<KMMTrackPanel> itt(m_trackList);

	while(itt.current() != 0) {
		itt.current()->docTrack().deleteClips(true);
		++itt;
	}
		
	drawTrackViewBackBuffer();
}

void KMMTimeLine::dropEvent ( QDropEvent *event )
{
	if(!m_selection.isEmpty()) {
		m_selection.setAutoDelete(true);
		m_selection.clear();
		m_selection.setAutoDelete(false);
	}
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

void KMMTimeLine::selectNone()
{
	QPtrListIterator<KMMTrackPanel> itt(m_trackList);

	while(itt.current()!=0) {
		itt.current()->docTrack().selectNone();
		++itt;
	}
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

bool KMMTimeLine::moveSelectedClips(int newTrack, GenTime start)
{
	int trackOffset = m_document->trackIndex(m_document->findTrack(m_masterClip));
	GenTime startOffset;

	if( (!m_masterClip) || (trackOffset==-1)) {
		kdError() << "Trying to move selected clips, master clip is not set." << endl;
		return false;
	} else {
		startOffset = m_masterClip->trackStart();
	}

	trackOffset = newTrack - trackOffset;
	startOffset = start - startOffset;


	// For each track, check and make sure that the clips can be moved to their rightful place. If
	// one cannot be moved, then none of them can be moved.
	int destTrackNum;
	DocTrackBase *srcTrack, *destTrack;
	GenTime clipStartTime;
	GenTime clipEndTime;
	DocClipBase *srcClip, *destClip;
	
	for(int track=0; track<m_trackList.count(); track++) {
		srcTrack = &m_trackList.at(track)->docTrack();
		if(!srcTrack->hasSelectedClips()) continue;

		destTrackNum = track + trackOffset;

		if((destTrackNum < 0) || (destTrackNum >= m_trackList.count())) return false;	// This track will be moving it's clips out of the timeline, so fail automatically.

		destTrack = &m_trackList.at(destTrackNum)->docTrack();

		QPtrListIterator<DocClipBase> srcClipItt = srcTrack->firstClip(true);
		QPtrListIterator<DocClipBase> destClipItt = destTrack->firstClip(false);

		destClip = destClipItt.current();

		while( (srcClip = srcClipItt.current()) != 0) {
			clipStartTime = srcClipItt.current()->trackStart() + startOffset;
			clipEndTime = clipStartTime + srcClipItt.current()->cropDuration();

			while((destClip) && (destClip->trackStart() + destClip->cropDuration() <= clipStartTime)) {
				++destClipItt;
				destClip = destClipItt.current();
			}
			if(destClip==0) break;

			if(destClip->trackStart() < clipEndTime) return false;

			++srcClipItt;
		}
	}

	// we can now move all clips where they need to be.

	// If the offset is negative, handle tracks from forwards, else handle tracks backwards. We
	// do this so that there are no collisions between selected clips, which would be caught by DocTrackBase
	// itself.
	
	int startAtTrack, endAtTrack, direction;
	
	if(trackOffset < 0) {
		startAtTrack = 0;
		endAtTrack = m_trackList.count();
		direction = 1;
	} else {
		startAtTrack = m_trackList.count() - 1;
		endAtTrack = -1;
		direction = -1;
	}

	for(int track=startAtTrack; track!=endAtTrack; track += direction) {
		srcTrack = &m_trackList.at(track)->docTrack();
		if(!srcTrack->hasSelectedClips()) continue;
		srcTrack->moveClips(startOffset, true);

		if(trackOffset) {
			destTrackNum = track + trackOffset;
			destTrack = &m_trackList.at(destTrackNum)->docTrack();
			destTrack->addClips(srcTrack->removeClips(true), true);
		}
	}
	
	drawTrackViewBackBuffer();
	return true;
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
void KMMTimeLine::toggleSelectClipAt(DocTrackBase &track, GenTime value)
{
	track.toggleSelectClip(track.getClipAt(value));
}

/** Selects the clip on the given track at the given value. */
void KMMTimeLine::selectClipAt(DocTrackBase &track, GenTime value)
{
	track.selectClip(track.getClipAt(value));
}

void KMMTimeLine::addClipsToTracks(DocClipBaseList &clips, int track, GenTime value, bool selected)
{
	if(clips.isEmpty()) return;

	if(selected) {
		selectNone();
	}

	DocClipBase *masterClip = clips.masterClip();
	if(!masterClip) masterClip = clips.first();

	GenTime startOffset = value - masterClip->trackStart();
	
	int trackOffset = masterClip->trackNum();
	if(trackOffset == -1) trackOffset = 0;
  trackOffset = track - trackOffset;
	
	QPtrListIterator<DocClipBase> itt(clips);
	int moveToTrack;

	while(itt.current() != 0) {
		moveToTrack = itt.current()->trackNum();

		if(moveToTrack==-1) {
			moveToTrack = track;
		} else {
			moveToTrack += trackOffset;
		}

		itt.current()->setTrackStart(itt.current()->trackStart() + startOffset);

		if((moveToTrack >=0) && (moveToTrack < m_trackList.count())) {
	    if(!m_trackList.at(moveToTrack)->docTrack().addClip(itt.current(), selected));
	  }

		++itt;
	}
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
void KMMTimeLine::initiateDrag(DocClipBase *clipUnderMouse, GenTime clipOffset)
{
	m_masterClip = clipUnderMouse;
	m_clipOffset = clipOffset;
	generateSnapToGridList();	
	
	m_startedClipMove = true;

	DocClipBaseList selection = listSelected();
	
	selection.setMasterClip(m_masterClip);	
	ClipDrag *clip = new ClipDrag(selection, this, "Timeline Drag");
	
	clip->dragCopy();
}

/** Sets a new time scale for the timeline. This in turn calls the correct kruler funtion and updates
the display. The scale is how many frames should fit into the space considered normal for 1 frame*/
void KMMTimeLine::setTimeScale(int scale)
{
	m_ruler->setValueScale(100.0 / scale);	
	drawTrackViewBackBuffer();
}

/** Returns true if the specified clip exists and is selected, false otherwise. If a track is
specified, we look at that track first, but fall back to a full search of tracks if the clip is
 not there. */
bool KMMTimeLine::clipSelected(DocClipBase *clip, DocTrackBase *track)
{
	if(track) {
		if(track->clipExists(clip)) {
			return track->clipSelected(clip);
		}
	}

	QPtrListIterator<KMMTrackPanel> itt(m_trackList);
	while(itt.current()) {
		if(itt.current()->docTrack().clipExists(clip)) {
			return itt.current()->docTrack().clipSelected(clip);
		}
	
		++itt;
	}

	return false;
}

bool KMMTimeLine::canAddClipsToTracks(DocClipBaseList &clips, int track, GenTime clipOffset)
{
	QPtrListIterator<DocClipBase> itt(clips);
	int numTracks = m_trackList.count();
	int trackOffset;
	GenTime startOffset;

	if(clips.masterClip()) {
		trackOffset = clips.masterClip()->trackNum();
		startOffset = clipOffset - clips.masterClip()->trackStart();
	} else {
		trackOffset = clips.first()->trackNum();
		startOffset = clipOffset - clips.first()->trackStart();
	}

	if(trackOffset==-1) trackOffset = 0;
	trackOffset = track - trackOffset;

	while(itt.current()) {
		itt.current()->setTrackStart(itt.current()->trackStart() + startOffset);
		++itt;
	}
	itt.toFirst();
	
	while(itt.current()) {
		int track = itt.current()->trackNum();
		if(track==-1) track = 0;
		track += trackOffset;

		if((track < 0) || (track >= numTracks)) {
			return false;
		}

		if(!m_trackList.at(track)->docTrack().canAddClip(itt.current())) {
			return false;
		}

		++itt;
	}

	return true;
}

/** Constructs a list of all clips that are currently selected. It does nothing else i.e.
it does not remove the clips from the timeline. */
DocClipBaseList KMMTimeLine::listSelected()
{
	DocClipBaseList list;

 	QPtrListIterator<KMMTrackPanel> itt(m_trackList);

  while(itt.current()) {
  	QPtrListIterator<DocClipBase> clipItt(itt.current()->docTrack().firstClip(true));

   	while(clipItt.current()) {
			list.inSort(clipItt.current());
			++clipItt;
		}
  	++itt;
  }
	
	return list;
}

/** Calculates the size of the project, and sets up the timeline to accomodate it. */
void KMMTimeLine::calculateProjectSize(double rulerScale)
{
	GenTime length;

	QPtrListIterator<KMMTrackPanel> itt(m_trackList);

	while(itt.current()) {
		GenTime test = itt.current()->docTrack().trackLength();
		length = (test > length) ? test : length;
		++itt;
	}	
	
  m_scrollBar->setRange(0, (int)(length.frames(25) * rulerScale) + m_scrollBar->width());
  m_ruler->setRange(0, (int)length.frames(25));  
}

void KMMTimeLine::generateSnapToGridList()
{
	m_snapToGridList.clear();

	QValueList<GenTime> list;

	QPtrListIterator<KMMTrackPanel> trackItt(m_trackList);

	while(trackItt.current()) {
		QPtrListIterator<DocClipBase> clipItt = trackItt.current()->docTrack().firstClip(false);
		while(clipItt.current()) {
			list.append(clipItt.current()->trackStart());
			list.append(clipItt.current()->trackEnd());
			++clipItt;
		}
		++trackItt;
	}

	GenTime masterClipOffset = m_masterClip->trackStart() + m_clipOffset;

	trackItt.toFirst();		
	while(trackItt.current()) {
		QPtrListIterator<DocClipBase> clipItt = trackItt.current()->docTrack().firstClip(true);
		while(clipItt.current()) {
			QValueListIterator<GenTime> timeItt = list.begin();

			while(timeItt != list.end()) {
				m_snapToGridList.append(masterClipOffset + (*timeItt) - clipItt.current()->trackStart());
				m_snapToGridList.append(masterClipOffset + (*timeItt) - clipItt.current()->trackEnd());				
				++timeItt;
			}			

			++clipItt;
		}
		++trackItt;
	}

  qHeapSort(m_snapToGridList);

  QValueListIterator<GenTime> itt = m_snapToGridList.begin();
  if(itt != m_snapToGridList.end()) {
	 	QValueListIterator<GenTime> next = itt;
	  ++next;
	  
	  while(next != m_snapToGridList.end()) {
	  	if((*itt) == (*next)) {
	   		m_snapToGridList.remove(next);
	   		next = itt;
				++next;
			} else {
				++itt;
				++next;
			}
	  }
	}

  m_gridSnapTracker = m_snapToGridList.begin();
}
