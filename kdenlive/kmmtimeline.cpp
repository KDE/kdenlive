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

#include <cmath>

#include <klocale.h>
#include <qscrollbar.h>
#include <qscrollview.h>
#include <qhbox.h>

#include <kdebug.h>

#include "doctrackclipiterator.h"
#include "kdenlivedoc.h"
#include "kmmtimeline.h"
#include "kmmtracksoundpanel.h"
#include "kmmtrackvideopanel.h"
#include "kmmtimelinetrackview.h"
#include "krulertimemodel.h"
#include "kscalableruler.h"
#include "kmoveclipscommand.h"
#include "kselectclipcommand.h"
#include "kaddclipcommand.h"
#include "kresizecommand.h"
#include "clipdrag.h"

int KMMTimeLine::snapTolerance=10;

KMMTimeLine::KMMTimeLine(KdenliveApp *app, QWidget *rulerToolWidget, QWidget *scrollToolWidget, KdenliveDoc *document, QWidget *parent, const char *name ) :
				QVBox(parent, name),
				m_document(document),
				m_selection()
{
	m_app = app;
	m_rulerBox = new QHBox(this, "ruler box");	
	m_trackScroll = new QScrollView(this, "track view", WPaintClever);
	m_scrollBox = new QHBox(this, "scroll box");
		
	m_rulerToolWidget = rulerToolWidget;
	if(!m_rulerToolWidget) m_rulerToolWidget = new QLabel("Tracks", 0, "Tracks");	
	m_rulerToolWidget->reparent(m_rulerBox, QPoint(0,0));
	m_ruler = new KScalableRuler(new KRulerTimeModel(), m_rulerBox, name);
	m_ruler->addSlider(KRuler::Diamond, 0);
	m_ruler->setAutoClickSlider(0);

	m_scrollToolWidget = scrollToolWidget;
	if(!m_scrollToolWidget) m_scrollToolWidget = new QLabel("Scroll", 0, "Scroll");	
	m_scrollToolWidget->reparent(m_scrollBox, QPoint(0,0));	
	m_scrollBar = new QScrollBar(-100, 5000, 50, 500, 0, QScrollBar::Horizontal, m_scrollBox, "horizontal ScrollBar");
	
	m_trackViewArea = new KMMTimeLineTrackView(*this, m_trackScroll, "track view area");
	
	m_trackScroll->enableClipper(TRUE);
	m_trackScroll->setVScrollBarMode(QScrollView::AlwaysOn);
	m_trackScroll->setHScrollBarMode(QScrollView::AlwaysOff);
	m_trackScroll->setDragAutoScroll(true);

	m_rulerToolWidget->setMinimumWidth(200);
	m_rulerToolWidget->setMaximumWidth(200);

	m_scrollToolWidget->setMinimumWidth(200);
	m_scrollToolWidget->setMaximumWidth(200);
		
	m_ruler->setValueScale(1.0);
	calculateProjectSize();
		
	connect(m_scrollBar, SIGNAL(valueChanged(int)), m_ruler, SLOT(setStartPixel(int)));
	connect(m_scrollBar, SIGNAL(valueChanged(int)), m_ruler, SLOT(repaint()));	
  connect(m_document, SIGNAL(trackListChanged()), this, SLOT(syncWithDocument()));
  connect(m_ruler, SIGNAL(scaleChanged(double)), this, SLOT(calculateProjectSize()));  
  connect(m_ruler, SIGNAL(sliderValueChanged(int, int)), m_trackViewArea, SLOT(invalidateBackBuffer()));
  connect(m_ruler, SIGNAL(sliderValueChanged(int, int)), m_ruler, SLOT(repaint()));
  connect(m_ruler, SIGNAL(sliderValueChanged(int, int)), this, SLOT(slotSliderMoved(int, int)));
  	
  setAcceptDrops(true);

	syncWithDocument();

	m_startedClipMove = false;
	m_masterClip = 0;
	m_moveClipsCommand = 0;
	m_deleteClipsCommand = 0;
	m_addingClips = false;

	m_trackList.setAutoDelete(true);
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
	connect(track->docTrack(), SIGNAL(trackChanged()), this, SLOT(drawTrackViewBackBuffer()));
		
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

	m_trackList.clear();
    
	DocTrackBase *track = m_document->firstTrack();
	while(track != 0) {
		if(track->clipType() == "Video") {
			insertTrack(index, new KMMTrackVideoPanel(this, ((DocTrackVideo *)track)));
			++index;
		} else if(track->clipType() == "Sound") {		
			insertTrack(index, new KMMTrackSoundPanel(this, ((DocTrackSound *)track)));
			++index;
		} else {
			kdWarning() << "Sync failed" << endl;
		}
		track = m_document->nextTrack();		
	}

	resizeTracks();
	calculateProjectSize();
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
		m_selection = ClipDrag::decode(m_document, event);

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
	GenTime mouseTime = timeUnderMouse((double)pos.x());

	if((m_app->snapToBorderEnabled()) && (m_gridSnapTracker != m_snapToGridList.end())) {
		QValueListIterator<GenTime> itt = m_gridSnapTracker;
		++itt;	
		while(itt != m_snapToGridList.end()) {
			if (fabs(((*itt) - mouseTime).seconds()) > fabs(((*m_gridSnapTracker) - mouseTime).seconds())) break;
			++m_gridSnapTracker;
			++itt;
		}

		itt = m_gridSnapTracker;
		--itt;
		while(m_gridSnapTracker != m_snapToGridList.begin()) {
			if (fabs(((*itt) - mouseTime).seconds()) > fabs(((*m_gridSnapTracker) - mouseTime).seconds())) break;
			--m_gridSnapTracker;
			--itt;
		}

		if( abs((int)mapValueToLocal((*m_gridSnapTracker).frames(m_document->framesPerSecond())) - pos.x()) < snapTolerance) {
			mouseTime = *m_gridSnapTracker;
		}
	}

	if(m_selection.isEmpty()) {
		moveSelectedClips(trackUnderPoint(pos), mouseTime - m_clipOffset);
	} else {
		if(canAddClipsToTracks(m_selection, trackUnderPoint(pos), mouseTime + m_clipOffset)) {
			addClipsToTracks(m_selection, trackUnderPoint(pos), mouseTime + m_clipOffset, true);
			generateSnapToGridList();
			m_selection.clear();			
		}
	}
	calculateProjectSize();
	m_trackViewArea->repaint();
}

void KMMTimeLine::dragLeaveEvent ( QDragLeaveEvent *event )
{
	m_addingClips = false;
	// In a drag Leave Event, any clips in the selection are removed from the timeline.			
	if(m_moveClipsCommand) {
  	delete m_moveClipsCommand;    
		m_moveClipsCommand = 0;
	}

	m_selection.clear();	
	
	QPtrListIterator<KMMTrackPanel> itt(m_trackList);

	if(m_deleteClipsCommand) {
		m_app->addCommand(m_deleteClipsCommand, false);
   	m_deleteClipsCommand = 0;
  }
  
	while(itt.current() != 0) {
		itt.current()->docTrack()->deleteClips(true);
		++itt;
	}

  calculateProjectSize();
	drawTrackViewBackBuffer();
}

void KMMTimeLine::dropEvent ( QDropEvent *event )
{
	if(!m_selection.isEmpty()) {
		m_selection.setAutoDelete(true);
		m_selection.clear();
		m_selection.setAutoDelete(false);
	}

	if(m_addingClips) {
		m_app->addCommand(createAddClipsCommand(true), false);
		m_addingClips = false;
	}

	if(m_deleteClipsCommand) {
		delete m_deleteClipsCommand;
		m_deleteClipsCommand = 0;
	}

	if(m_moveClipsCommand) {
		m_moveClipsCommand->setEndLocation(m_masterClip);
		m_app->addCommand(m_moveClipsCommand, false);
		m_moveClipsCommand = 0;	// KdenliveApp is now managing this command, we do not need to delete it.
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

KCommand *KMMTimeLine::selectNone()
{
	QPtrListIterator<KMMTrackPanel> itt(m_trackList);

	KMacroCommand *command = new KMacroCommand(i18n("Selection"));

	while(itt.current()!=0) {
		QPtrListIterator<DocClipBase> clipItt(itt.current()->docTrack()->firstClip(true));
		while(clipItt.current()!=0) {
			KSelectClipCommand *clipComm = new KSelectClipCommand(m_document, clipItt.current(), false);
			command->addCommand(clipComm);
			++clipItt;
		}
		++itt;
	}

	return command;
}

void KMMTimeLine::drawTrackViewBackBuffer()
{
	m_trackViewArea->invalidateBackBuffer();
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

	m_document->moveSelectedClips(startOffset, trackOffset);

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
	DocClipBase *clip = track.getClipAt(value);
	if(clip) {
		KSelectClipCommand *command = new KSelectClipCommand(m_document, clip, !track.clipSelected(clip));
		m_app->addCommand(command, true);
	}
}

/** Selects the clip on the given track at the given value. */
void KMMTimeLine::selectClipAt(DocTrackBase &track, GenTime value)
{
	DocClipBase *clip = track.getClipAt(value);
	if(clip) {
		KSelectClipCommand *command = new KSelectClipCommand(m_document, clip, true);
		m_app->addCommand(command, true);
	}
}

void KMMTimeLine::addClipsToTracks(DocClipBaseList &clips, int track, GenTime value, bool selected)
{
	if(clips.isEmpty()) return;

	if(selected) {
		addCommand(selectNone(), true);
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

		itt.current()->moveTrackStart(itt.current()->trackStart() + startOffset);

		if((moveToTrack >=0) && (moveToTrack < m_trackList.count())) {		
	    m_trackList.at(moveToTrack)->docTrack()->addClip(itt.current(), selected);	    
	  }

		++itt;
	}

	m_addingClips = true;
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
	m_moveClipsCommand = new KMoveClipsCommand(this, m_document, m_masterClip);
	m_deleteClipsCommand = createAddClipsCommand(false);
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
	int localValue = (int)mapValueToLocal(m_ruler->getSliderValue(0));	

  double frameScale = 100.0 / scale;
	
	m_ruler->setValueScale(frameScale);

	m_scrollBar->setValue((int)(frameScale*m_ruler->getSliderValue(0)) - localValue);	
	
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
		if(itt.current()->docTrack()->clipExists(clip)) {
			return itt.current()->docTrack()->clipSelected(clip);
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
		itt.current()->moveTrackStart(itt.current()->trackStart() + startOffset);
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

		if(!m_trackList.at(track)->docTrack()->canAddClip(itt.current())) {
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
  	QPtrListIterator<DocClipBase> clipItt(itt.current()->docTrack()->firstClip(true));

   	while(clipItt.current()) {
			list.inSort(clipItt.current());
			++clipItt;
		}
  	++itt;
  }
	
	return list;
}

/** Calculates the size of the project, and sets up the timeline to accomodate it. */
void KMMTimeLine::calculateProjectSize()
{
	double rulerScale = m_ruler->valueScale();
	GenTime length;

	QPtrListIterator<KMMTrackPanel> itt(m_trackList);

	while(itt.current()) {
		GenTime test = itt.current()->docTrack()->trackLength();
		length = (test > length) ? test : length;
		++itt;
	}	
	
  m_scrollBar->setRange(0, (int)(length.frames(m_document->framesPerSecond()) * rulerScale) + m_scrollBar->width());
  m_ruler->setRange(0, (int)length.frames(m_document->framesPerSecond()));

  emit projectLengthChanged((int)length.frames(m_document->framesPerSecond()));
}

void KMMTimeLine::generateSnapToGridList()
{
	m_snapToGridList.clear();

	QValueList<GenTime> list;

	QPtrListIterator<KMMTrackPanel> trackItt(m_trackList);

	while(trackItt.current()) {
		QPtrListIterator<DocClipBase> clipItt = trackItt.current()->docTrack()->firstClip(false);
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
		QPtrListIterator<DocClipBase> clipItt = trackItt.current()->docTrack()->firstClip(true);
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

GenTime KMMTimeLine::seekPosition()
{
	return GenTime(m_ruler->getSliderValue(0), m_document->framesPerSecond());
}

KMacroCommand *KMMTimeLine::createAddClipsCommand(bool addingClips)
{
	KMacroCommand *macroCommand = new KMacroCommand( addingClips ? i18n("Add Clips") : i18n("Delete Clips") );

	for(int count=0; count<m_document->numTracks(); count++) {
		DocTrackBase *track = m_document->track(count);

		QPtrListIterator<DocClipBase> itt = track->firstClip(true);

		while(itt.current()) {
			KAddClipCommand *command = new KAddClipCommand(m_document, itt.current(), addingClips);
			macroCommand->addCommand(command);
			++itt;
		}		
	}
	
	return macroCommand;
}

void KMMTimeLine::addCommand(KCommand *command, bool execute)
{
	m_app->addCommand(command, execute);
}

KdenliveApp::TimelineEditMode KMMTimeLine::editMode()
{
	return m_app->timelineEditMode();
}

KCommand *KMMTimeLine::razorAllClipsAt(GenTime time)
{
	return 0;
}

KCommand *KMMTimeLine::razorClipAt(DocTrackBase &track, GenTime &time)
{
	DocClipBase *clip = track.getClipAt(time);
	if(!clip) return 0;

	if((clip->trackStart() == time) || (clip->trackEnd() == time)) return 0;	// disallow the creation of clips with 0 length.
	
	KMacroCommand *command = new KMacroCommand(i18n("Razor clip"));
	
	command->addCommand(selectNone());

  DocClipBase *clone = clip->clone();
    
	clone->setTrackStart(time);
  clone->setCropStartTime(clip->cropStartTime() + (time - clip->trackStart()));

  command->addCommand(resizeClip(clip, true, time));
  command->addCommand(new KAddClipCommand(m_document, clone, true));

  delete clone;

  return command;	
}

KCommand * KMMTimeLine::resizeClip(DocClipBase *clip, bool resizeEnd, GenTime &time)
{
	KResizeCommand *command = new KResizeCommand(m_document, clip);
	
	if(resizeEnd) {
		command->setEndTrackEnd(time);
	} else {
		command->setEndTrackStart(time);
	}

	kdDebug() << "Command is " << command << endl;
	return command;
}

/** Selects all of the clips on the timeline which occur later than the
specified time. If include is true, then clips which overlap the
specified time will be selected, otherwise only those clips which
are later on the tiemline (i.e. trackStart() > time) will be selected. */
KCommand * KMMTimeLine::selectLaterClips(GenTime time, bool include)
{
	KMacroCommand *command = new KMacroCommand(i18n("Selection"));

	QPtrListIterator<KMMTrackPanel> itt(m_trackList);

	bool select;
	
	while(itt.current()!=0) {
		DocTrackClipIterator clipItt(*(itt.current()->docTrack()));
		while(clipItt.current()!=0) {
			if(include) {
				select = clipItt.current()->trackEnd() > time;
			} else {
				select = clipItt.current()->trackStart() > time;
			}
			KSelectClipCommand *clipComm = new KSelectClipCommand(m_document, clipItt.current(), select);
			command->addCommand(clipComm);
			++clipItt;
		}
		++itt;
	}

	return command;
}

/** A ruler slider has moved - do something! */
void KMMTimeLine::slotSliderMoved(int slider, int value)
{
  if(slider == 0) {
    emit seekPositionChanged(GenTime(value, m_document->framesPerSecond()));
  }
}

/** Seek the timeline to the current position. */
void KMMTimeLine::seek(GenTime time)
{
    m_ruler->setSliderValue(0, (int)floor(time.frames(m_document->framesPerSecond()) + 0.5));
}

/** Returns the correct "time under mouse", taking into account whether or not snap to frame is on or off, and other relevant effects. */
GenTime KMMTimeLine::timeUnderMouse(double posX)
{
  GenTime value(m_ruler->mapLocalToValue(posX), m_document->framesPerSecond());

  if(m_app->snapToFrameEnabled()) value = GenTime(floor(value.frames(m_document->framesPerSecond()) + 0.5), m_document->framesPerSecond());

  return value;
}
