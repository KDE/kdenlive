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
#include <iostream>
#include <stdlib.h>
#include <iostream>
#include <assert.h>

#include <qscrollbar.h>
#include <qscrollview.h>
#include <qbuttongroup.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qrect.h>
#include <qpainter.h>

#include <klocale.h>
#include <kinputdialog.h>
#include <krestrictedline.h>
#include <kdebug.h>

#include "krulertimemodel.h"
#include "ktimeline.h"
#include "ktrackview.h"
#include "kresizecommand.h"
#include "kscalableruler.h"
#include "kdenlive.h"
#include "kmmtrackkeyframepanel.h"
#include "addmarker_ui.h"
#include "timecode.h"


namespace Gui {

    uint KTimeLine::snapTolerance = 10;

     KTimeLine::KTimeLine(QWidget * rulerToolWidget,
	QWidget * scrollToolWidget, QWidget * parent,
	const char *name):QVBox(parent, name), m_scrollTimer(this,
	"scroll timer"), m_scrollingRight(true), m_framesPerSecond(KdenliveSettings::defaultfps()),
	m_editMode("undefined"), m_panelWidth(120), m_selectedTrack(0) {
	m_rulerBox = new QHBox(this, "ruler box");
        m_trackScroll = new QScrollView(this, "track view", WPaintClever);
	m_scrollBox = new QHBox(this, "scroll box");

	m_rulerToolWidget = rulerToolWidget;
	if (!m_rulerToolWidget)
	    m_rulerToolWidget = new QLabel(i18n("Ruler"), 0, "ruler");
	m_rulerToolWidget->reparent(m_rulerBox, QPoint(0, 0));

	m_ruler =
	    new KScalableRuler(new KRulerTimeModel(), m_rulerBox, "timeline_ruler");
	m_ruler->addSlider(KRuler::TopMark, 0);
	ruler_tips = new DynamicToolTip(m_ruler);
	//added inpoint/outpoint markers -reh
	m_ruler->addSlider(KRuler::StartMark, 0);
	m_ruler->addSlider(KRuler::EndMark, m_ruler->maxValue());
	m_ruler->addSlider(KRuler::HorizontalMark,
	    m_ruler->maxValue() / 2);
	m_ruler->setAutoClickSlider(0);

	m_scrollToolWidget = scrollToolWidget;
	if (!m_scrollToolWidget)
	    m_scrollToolWidget = new QLabel(i18n("Scroll"), 0, "Scroll");
	m_scrollToolWidget->reparent(m_scrollBox, QPoint(0, 0));
	m_scrollBar =
	    new QScrollBar(-100, 5000, 40, 500, 0, QScrollBar::Horizontal,
	    m_scrollBox, "horizontal ScrollBar");

	m_trackViewArea = new KTrackView(*this, m_framesPerSecond, m_trackScroll, "trackview_area");
	m_trackScroll->enableClipper(TRUE);
	m_trackScroll->setFrameStyle (QFrame::NoFrame);
	m_trackScroll->setHScrollBarMode(QScrollView::AlwaysOff);
        m_trackScroll->setDragAutoScroll(true);

	setPanelWidth(120);

	m_ruler->setValueScale(1.0);

	connect(m_scrollBar, SIGNAL(valueChanged(int)), m_ruler,
	    SLOT(setStartPixel(int)));
	 connect(m_scrollBar, SIGNAL(valueChanged(int)), m_ruler,
	    SLOT(update()));

	 connect(m_scrollBar, SIGNAL(valueChanged(int)), this,
	    SLOT(drawTrackViewBackBuffer()));
         
	 connect(m_ruler, SIGNAL(scaleChanged(double)), this,
	    SLOT(resetProjectSize()));
	 connect(m_ruler, SIGNAL(sliderValueMoved(int, int)),
	    m_trackViewArea, SLOT(invalidatePartialBackBuffer(int, int)));

	 connect(m_ruler, SIGNAL(sliderValueChanged(int, int)), this,
                 SLOT(slotSliderMoved(int, int)));
         
         connect(m_ruler, SIGNAL(moveForward(bool)), this, SLOT(slotMoveForward(bool)));
         connect(m_ruler, SIGNAL(moveBackward(bool)), this, SLOT(slotMoveBackward(bool)));

	 connect(m_ruler, SIGNAL(requestScrollLeft()), this,
	    SLOT(slotScrollLeft()));
	 connect(m_ruler, SIGNAL(requestScrollRight()), this,
	    SLOT(slotScrollRight()));
	 connect(&m_scrollTimer, SIGNAL(timeout()), this,
	    SLOT(slotTimerScroll()));

	 connect(m_trackViewArea, SIGNAL(rightButtonPressed()), this,
	    SIGNAL(rightButtonPressed()));

	 connect(m_ruler, SIGNAL(rightButtonPressed()), this,
	    SIGNAL(rulerRightButtonPressed()));

	 setAcceptDrops(true);
	 m_trackList.setAutoDelete(true);
    } 
    
    KTimeLine::~KTimeLine() {
	delete ruler_tips;
    }

    void KTimeLine::slotHeaderRightButtonPressed() {
	emit headerRightButtonPressed();
    }

    void KTimeLine::appendTrack(KTrackPanel * track) {
	if (track) {
	    insertTrack(m_trackList.count(), track);
	} else {
	    kdWarning() <<
		"KTimeLine::appendTrack() : Trying to append a NULL track!"
		<< endl;
	}
    }

    
/** Inserts a track at the position specified by index */
    void KTimeLine::insertTrack(int index, KTrackPanel * track) {
	assert(track);
	track->reparent(m_trackScroll->viewport(), 0, QPoint(0, 0), TRUE);
	m_trackScroll->addChild(track);
	connect(track, SIGNAL(collapseTrack(KTrackPanel *, bool)), this,
	    SLOT(collapseTrack(KTrackPanel *, bool)));
	m_trackList.insert(index, track);
	resizeTracks();
    }

    void KTimeLine::resizeEvent(QResizeEvent * event) {
	resizeTracks();
    }

    void KTimeLine::collapseTrack(KTrackPanel * panel, bool collapse) {
	/*
	// if we collapse a video track, also collapse its transition track...
	if (panel->trackType() == VIDEOTRACK) {
	    KTrackPanel *pane = m_trackList.first();
	    while (pane) {
		if (pane == panel) {
		    pane = m_trackList.next();
		    static_cast < KMMTrackKeyFramePanel *>(pane)->resizeTrack();
		    break;
		}
		pane = m_trackList.next();
	    }
	}*/
	resizeTracks();
    }
    

    void KTimeLine::resizeTracks() {
	int height = 0;

	// default height for keyframe tracks
	int widgetHeight;

	KTrackPanel *panel = m_trackList.first();

	while (panel) {

	    if (panel->trackType() == KEYFRAMETRACK) {
		if (panel->isTrackCollapsed())
		    widgetHeight = 0;
		else
		    widgetHeight = 20;
	    } else if (panel->trackType() == EFFECTKEYFRAMETRACK) {
		if (panel->isTrackCollapsed())
		    widgetHeight = 0;
		else
		    widgetHeight = 50;
	    } else if (panel->isTrackCollapsed())
		widgetHeight = collapsedTrackSize;
	    else if (panel->trackType() == VIDEOTRACK)
		widgetHeight = KdenliveSettings::videotracksize();
	    else if (panel->trackType() == SOUNDTRACK)
		widgetHeight = KdenliveSettings::audiotracksize();

	    m_trackScroll->moveChild(panel, 0, height);
	    panel->resize(m_panelWidth, widgetHeight);
	    height += widgetHeight;
	    panel = m_trackList.next();

	}


	m_trackScroll->moveChild(m_trackViewArea, m_panelWidth, 0);
	int newWidth = m_trackScroll->visibleWidth() - m_panelWidth;
	if (newWidth < 0)
	    newWidth = 0;
	m_trackViewArea->resize(newWidth, height);

	m_trackScroll->resizeContents(m_trackScroll->visibleWidth(),
	    height);
    }

/** No descriptions */
    void KTimeLine::polish() {
	resizeTracks();
    }

/** This method maps a local coordinate value to the corresponding
value that should be represented at that position. By using this, there is no need to calculate scale factors yourself. Takes the x
coordinate, and returns the value associated with it. */
    double KTimeLine::mapLocalToValue(const double coordinate) const {
	return m_ruler->mapLocalToValue(coordinate);
    }
/** Takes the value that we wish to find the coordinate for, and returns the x coordinate. In cases where a single value covers multiple
pixels, the left-most pixel is returned. */
    double KTimeLine::mapValueToLocal(const double value) const {
	return m_ruler->mapValueToLocal(value);
    } 
    
    void KTimeLine::drawTrackViewBackBuffer(int startTrack, int endTrack) {
	m_trackViewArea->invalidateBackBuffer(startTrack, endTrack);
    }

    void KTimeLine::drawPartialTrackViewBackBuffer(int start, int end, int startTrack, int endTrack)
    {
	m_trackViewArea->invalidatePartialBackBuffer(start, end, startTrack, endTrack);
    }

    void KTimeLine::drawCurrentTrack(int track, int offset, GenTime start, GenTime end) {
	if (track == -1) {
	    drawTrackViewBackBuffer();
	}
	else {
	    if (start != GenTime(0) && end != GenTime(0)) {
		if (offset >= 0) drawPartialTrackViewBackBuffer(start.frames(m_framesPerSecond), end.frames(m_framesPerSecond), 2 * (track - offset), 2 * track + 1);
	    	else drawPartialTrackViewBackBuffer(start.frames(m_framesPerSecond), end.frames(m_framesPerSecond), 2 * track, 2 * (track - offset) + 1);
	    } else {
		if (offset >= 0) drawTrackViewBackBuffer(2 * (track - offset), 2 * track + 1);
	    	else drawTrackViewBackBuffer(2 * track, 2 * (track - offset) + 1);
	    }
	}
    }

    void KTimeLine::drawPartialTrack(int track, GenTime start, GenTime end)
    {
	if (track == -1) {
	    drawTrackViewBackBuffer();
	}
	else {
	    if (start != GenTime(0) && end != GenTime(0)) {
	    	 drawPartialTrackViewBackBuffer(start.frames(m_framesPerSecond), end.frames(m_framesPerSecond), 2 * track, 2 * track + 1);
	    } else {
	    	drawTrackViewBackBuffer(2 * track, 2 * track + 1);
	    }
	}
    }

/** Returns m_trackList

Warning - this method is a bit of a hack, not good OO practice, and should be removed at some point. */
    QPtrList < KTrackPanel > &KTimeLine::trackList() {
	return m_trackList;
    }


/** Sets a new time scale for the timeline. This in turn calls the correct kruler funtion and updates
the display. The scale is the size of one frame.*/
    void KTimeLine::setTimeScale(double scale) {
	int localValue = (int) mapValueToLocal(m_ruler->getSliderValue(0));
	if (localValue < 0 || localValue > (int) mapValueToLocal(viewWidth())) {
	  ensureCursorVisible();
	  localValue = (int) mapValueToLocal(m_ruler->getSliderValue(0));
	}
	m_ruler->setValueScale(scale);
	m_scrollBar->setValue((int) (scale * m_ruler->getSliderValue(0)) -
	    localValue);

	drawTrackViewBackBuffer();
    }
    
    double KTimeLine::timeScale() {
        return m_ruler->valueScale();
    }

    int KTimeLine::selectedTrack() {
	return m_selectedTrack;
    }

    void KTimeLine::selectPreviousTrack() {
	m_selectedTrack--;
	int max = trackList().count() / 2 - 1;
	if ( m_selectedTrack < 0 ) m_selectedTrack = max;
	drawTrackViewBackBuffer(0, trackList().count() - 1);
	KTrackPanel *panel = m_trackList.first();
	while (panel) {
		bool isSelected = false;
		if (panel->documentTrackIndex() == m_selectedTrack) {
			m_trackScroll->ensureVisible ( localSeekPosition(), panel->y() - 2000);
			isSelected = true;
		}
		panel->setSelected(isSelected);
		panel = m_trackList.next();
	}
    }

    void KTimeLine::selectNextTrack() {
	m_selectedTrack++;
	int max = trackList().count() / 2 - 1;
	if ( m_selectedTrack > max) m_selectedTrack = 0;
	drawTrackViewBackBuffer(0, trackList().count() - 1);

	KTrackPanel *panel = m_trackList.first();
	while (panel) {
		bool isSelected = false;
		if (panel->documentTrackIndex() == m_selectedTrack) {
			m_trackScroll->ensureVisible ( localSeekPosition(), panel->y() - 2000);
			isSelected = true;
		}
		panel->setSelected(isSelected);
		panel = m_trackList.next();
	}
    }

    void KTimeLine::selectTrack(int ix) {
	int prev_track = m_selectedTrack;
	m_selectedTrack = ix;
	drawTrackViewBackBuffer(0, trackList().count() - 1);
	/*if (prev_track < ix) drawTrackViewBackBuffer(prev_track, ix - prev_track);
	else drawTrackViewBackBuffer(ix, prev_track - ix);*/

	KTrackPanel *panel = m_trackList.first();
	while (panel) {
		bool isSelected = false;
		if (panel->documentTrackIndex() == m_selectedTrack)
			isSelected = true;
		panel->setSelected(isSelected);
		panel = m_trackList.next();
	}
    }

/** Calculates the size of the project, and sets up the timeline to accomodate it. */
    void KTimeLine::slotSetProjectLength(const GenTime & size) {
	int previous_duration = m_ruler->maxValue();
        int frames = (int) size.frames( m_framesPerSecond);
	m_scrollBar->setRange(0, (int) (frames * m_ruler->valueScale()) + m_scrollBar->width());
	m_ruler->setRange(0, frames);
	emit projectLengthChanged(frames);
    }

    void KTimeLine::resetProjectSize() {
	m_scrollBar->setRange(0, ((int) (m_ruler->maxValue() * m_ruler->valueScale())) + m_scrollBar->width());
    }

    GenTime KTimeLine::seekPosition() const {
	return GenTime(m_ruler->getSliderValue(0), m_framesPerSecond);
    }

    int KTimeLine::localSeekPosition() const {
	return m_ruler->getSliderValue(0);
    }

void KTimeLine::autoScroll() {
  int max = (int) mapLocalToValue(viewWidth());
  int min = max - (max - mapLocalToValue(0))/2.4;
	// Only scroll if the cursor is in the right part of the timeline and 
	// zoom factor is lower than 2 frames 
	if (seekPosition().frames( m_framesPerSecond ) < min || seekPosition().frames( m_framesPerSecond ) > max || timeScale()>20) return;
	int step = 1;
  if (timeScale() > 1) step = (int) timeScale();
	m_scrollBar->setValue( m_scrollBar->value() + step );
    }

void KTimeLine::ensureCursorVisible() {
  int max =(int) mapLocalToValue(viewWidth());
  int min = (int) mapLocalToValue(0);

	// Only scroll if the cursor is out of view
	if (seekPosition().frames( m_framesPerSecond ) < min ) {
    int diff = (min -  seekPosition().frames(m_framesPerSecond) + (max - min) / 3) * timeScale();
		m_scrollBar->setValue( m_scrollBar->value() - diff );
	}
	else if ( seekPosition().frames( m_framesPerSecond ) > max) {
    int diff = (seekPosition().frames(m_framesPerSecond ) - max + (max - min) / 3) * timeScale();
		m_scrollBar->setValue( m_scrollBar->value() + diff );
	}
    }
    
    void KTimeLine::slotMoveForward(bool fast)
    {
        GenTime t = seekPosition();
        if (fast) t += GenTime( (int) m_framesPerSecond, m_framesPerSecond);
        else t += GenTime(1, m_framesPerSecond);
        seek(t);
	ensureCursorVisible();
    }
    
    void KTimeLine::slotMoveBackward(bool fast)
    {
        GenTime t = seekPosition();
        if (fast) t = t - GenTime( (int) m_framesPerSecond, m_framesPerSecond);
        else t = t - GenTime(1, m_framesPerSecond);
        seek(t);
	ensureCursorVisible();
    }
    
//returns inpoint/outpoint timeline positions -reh
    GenTime KTimeLine::inpointPosition() const {
	return GenTime(m_ruler->getSliderValue(1), m_framesPerSecond);
    } 
    
    GenTime KTimeLine::outpointPosition() const {
	return GenTime(m_ruler->getSliderValue(2), m_framesPerSecond);
    } 
    
    GenTime KTimeLine::midpointPosition() const {
	return GenTime(m_ruler->getSliderValue(3), m_framesPerSecond);
    } 
    
    void KTimeLine::setMidValueDiff() {
	if (m_ruler->activeSliderID() != 3) {
	    if (m_ruler->activeSliderID() == 1) {
		m_midPoint = midpointPosition() - inpointPosition();
	    } else if (m_ruler->activeSliderID() == 2) {
		m_midPoint = outpointPosition() - midpointPosition();
	    } else {
		m_midPoint = outpointPosition() - midpointPosition();
	    }
	}
    }

    void KTimeLine::setEditMode(const QString & editMode) {
	m_editMode = editMode;
    }

    const QString & KTimeLine::editMode() const {
	return m_editMode;
    }
    
/** A ruler slider has moved - do something! */
	void KTimeLine::slotSliderMoved(int slider, int value) {
	switch (slider) {
	case 0:
            emit seekPositionChanged(GenTime(value, m_framesPerSecond));
	    break;
	case 1:
	    emit inpointPositionChanged(GenTime(value, m_framesPerSecond));
	    if (m_ruler->activeSliderID() != 3) {
		horizontalSlider(inpointPosition(), outpointPosition());
	    }
	    setMidValueDiff();
	    if (value >= m_ruler->getSliderValue(2)) {
		m_ruler->setSliderValue(2, value + 8);
	    }
	    break;
	case 2:
	    emit outpointPositionChanged(GenTime(value,
		    m_framesPerSecond));
	    if (m_ruler->activeSliderID() != 3) {
		horizontalSlider(inpointPosition(), outpointPosition());
	    }
	    setMidValueDiff();
	    if (m_ruler->getSliderValue(1) >= value) {
		m_ruler->setSliderValue(1, value - 8);
	    }
	    break;
	case 3:
	    if (m_ruler->activeSliderID() != 1
		&& m_ruler->activeSliderID() != 2) {
		if (m_ruler->activeSliderID() == 3) {
		    m_ruler->setSliderValue(1,
			(int) floor((midpointPosition() -
                                m_midPoint).frames(m_framesPerSecond)));
		    m_ruler->setSliderValue(2,
			(int) floor((midpointPosition() +
                                m_midPoint).frames(m_framesPerSecond)));
		}
	    }
	    break;
	default:
	    break;
	}
    }

/** Seek the timeline to the current position. */
    void KTimeLine::seek(const GenTime & time) {
	m_ruler->setSliderValue(0, (int) floor(time.frames(m_framesPerSecond) + 0.5));
    }

    void KTimeLine::horizontalSlider(const GenTime & inpoint,
	const GenTime & outpoint) {
	int midValue =
	    (int) floor(outpoint.frames(m_framesPerSecond)) -
                (int) floor(inpoint.frames(m_framesPerSecond));
	midValue =
                midValue / 2 + (int) floor(inpoint.frames(m_framesPerSecond));
	m_ruler->setSliderValue(3, midValue);
    }

/** Returns the correct "time under mouse", taking into account whether or not snap to frame is on or off, and other relevant effects. */
GenTime KTimeLine::timeUnderMouse(double posX) {

  GenTime value( (int) m_ruler->mapLocalToValue(posX), m_framesPerSecond);

	if (snapToFrame())
	    value = GenTime(floor(value.frames(m_framesPerSecond)) , m_framesPerSecond);

	return value;
    }

    bool KTimeLine::snapToBorders() const {
	return m_snapToBorder;
    } 
    
    bool KTimeLine::snapToFrame() const {
	return m_snapToFrame;
    } 
    
    bool KTimeLine::snapToMarkers() const {
	return m_snapToMarker;
    } 
    
    bool KTimeLine::snapToSeekTime() const {
#warning snapToSeekTime always returns true - needs to be wired up in KdenliveApp to
#warning a check box.
	return true;
    } 
    
    GenTime KTimeLine::projectLength() const {
	return GenTime(m_ruler->maxValue(), m_framesPerSecond);
    } 
    
    void KTimeLine::slotTimerScroll() {
	if (m_scrollingRight) {
	    m_scrollBar->addLine();
	} else {
	    m_scrollBar->subtractLine();
	}
    }

    void KTimeLine::placeScrollBar(int pos) {
        m_scrollBar->setValue(pos);
    }
    
    int KTimeLine::scrollBarPosition() {
        return m_scrollBar->value();
    }
    
    void KTimeLine::slotScrollLeft() {
	m_scrollBar->subtractLine();
    }

    void KTimeLine::slotScrollRight() {
	m_scrollBar->addLine();
    }

    void KTimeLine::slotScrollUp() {
	m_trackScroll->verticalScrollBar()->subtractLine();
    }

    void KTimeLine::slotScrollDown() {
	m_trackScroll->verticalScrollBar()->addLine();
    }

    void KTimeLine::clearTrackList() {
	m_trackList.clear();
	resizeTracks();
    }

    uint KTimeLine::scrollThreshold() const {
	return g_scrollThreshold;
    } 
    
    uint KTimeLine::scrollTimerDelay() const {
	return g_scrollTimerDelay;
    } 
    
    void KTimeLine::checkScrolling(const QPoint & pos) {
      if (pos.x() < (int) scrollThreshold()) {
	    if (!m_scrollTimer.isActive()) {
		m_scrollTimer.start(scrollTimerDelay(), false);
	    }
	    m_scrollingRight = false;
      } else if (m_trackViewArea->width() - pos.x() < (int) scrollThreshold()) {
	    if (!m_scrollTimer.isActive()) {
		m_scrollTimer.start(scrollTimerDelay(), false);
	    }
	    m_scrollingRight = true;
	} else {
	    m_scrollTimer.stop();
	}
    }

    void KTimeLine::stopScrollTimer() {
	m_scrollTimer.stop();
    }

    void KTimeLine::slotSetFramesPerSecond(double fps) {
	m_framesPerSecond = fps;
    }

    void KTimeLine::setSnapToFrame(bool snapToFrame) {
	m_snapToFrame = snapToFrame;
    }

    void KTimeLine::setSnapToBorder(bool snapToBorder) {
	m_snapToBorder = snapToBorder;
    }

    void KTimeLine::setSnapToMarker(bool snapToMarker) {
	m_snapToMarker = snapToMarker;
    }

    int KTimeLine::viewWidth() const {
	return m_trackViewArea->width();
    } 
    
    void KTimeLine::setPanelWidth(int width) {
	m_panelWidth = width;

	m_rulerToolWidget->setMinimumWidth(m_panelWidth);
	m_rulerToolWidget->setMaximumWidth(m_panelWidth);

	m_scrollToolWidget->setMinimumWidth(m_panelWidth);
	m_scrollToolWidget->setMaximumWidth(m_panelWidth);

	resizeTracks();
    }

    void KTimeLine::setInpointTimeline(const GenTime & inpoint) {
	m_ruler->setSliderValue(1,
	    (int) floor(inpoint.frames(m_framesPerSecond)));
    }

    void KTimeLine::setOutpointTimeline(const GenTime & outpoint) {
	m_ruler->setSliderValue(2,
	    (int) floor(outpoint.frames(m_framesPerSecond)));
    }

    QDomDocument KTimeLine::xmlGuides() {
	return m_ruler->xmlGuides();
    }

    void KTimeLine::guidesFromXml(QDomElement doc) {
	m_ruler->guidesFromXml(doc);
    }

    QValueList < int > KTimeLine::timelineGuides() {
	return m_ruler->timelineGuides();
    }

    QStringList KTimeLine::timelineRulerComments() {
	return m_ruler->timelineRulerComments();
    }

    GenTime KTimeLine::guideTime(int ix) {
	int pos = *(m_ruler->timelineGuides().at(ix));
	return GenTime(pos, m_framesPerSecond);
    }

    void KTimeLine::slotSetVZone(QValueList < QPoint > zones) {
	m_ruler->slotSetVZone(zones);
	m_ruler->invalidateBackBuffer();
    }

    void KTimeLine::slotAddGuide() {
	AddMarker_UI dlg;
	Timecode tcode;
	if (m_framesPerSecond == 30000.0 / 1001.0 ) tcode.setFormat(30, true);
        else tcode.setFormat(m_framesPerSecond);
	dlg.setCaption(i18n("Add Guide"));
	GenTime pos = GenTime(m_ruler->getSliderValue(0), m_framesPerSecond);
	dlg.marker_position->setText(tcode.getTimecode(pos, m_framesPerSecond));
	if (dlg.exec() == QDialog::Accepted) {
	    QString dur = dlg.marker_position->text();
            int frames = tcode.getFrameNumber(dur, m_framesPerSecond);
	    int chapter = -1;
	    if (dlg.dvd_marker->isChecked()) {
	        if (dlg.chapter_start->isChecked()) chapter = 0;
	        else if (dlg.chapter_end->isChecked()) chapter = 1000;
	    }
	    m_ruler->slotAddGuide(frames, dlg.marker_comment->text(), chapter);
	    trackView()->invalidatePartialBackBuffer(frames - 7, frames + 7);
	}
    }

    void KTimeLine::slotDeleteGuide() {
	m_ruler->slotDeleteGuide();
	trackView()->invalidatePartialBackBuffer(m_ruler->getSliderValue(0) - 2, m_ruler->getSliderValue(0) + 2);
    }

    void KTimeLine::insertSilentGuide(int frame, QString comment) {
        m_ruler->slotAddGuide(frame, comment);
    }

    void KTimeLine::slotEditGuide() {
	int ix = m_ruler->currentGuideIndex();
	if (ix == -1) {
	    kdDebug()<<" NO GUIDE FOUND UNDER TIMELINE POSITON"<<endl;
	    return;
	}
	QString comment = m_ruler->guideComment(ix);
	int pos = m_ruler->guidePosition(ix);
	AddMarker_UI dlg;
	dlg.setCaption(i18n("Edit Guide"));
	Timecode tcode;
	if (m_framesPerSecond == 30000.0 / 1001.0 ) tcode.setFormat(30, true);
        else tcode.setFormat(m_framesPerSecond);
	GenTime position = GenTime(pos, m_framesPerSecond);
	dlg.marker_position->setText(tcode.getTimecode(position, m_framesPerSecond));
	dlg.marker_comment->setText(comment);
	int chap = m_ruler->guideChapter(ix);
	if (chap > -1) {
	    dlg.dvd_marker->setChecked(true);
	    if (chap == 1000)
		dlg.chapter_end->setChecked(true);
	    else {
		dlg.chapter_start->setChecked(true);
	    }
	}
	if (dlg.exec() == QDialog::Accepted) {
	    QString dur = dlg.marker_position->text();
            int frames = tcode.getFrameNumber(dur, m_framesPerSecond);
    	    int chapter = -1;
	    if (dlg.dvd_marker->isChecked()) {
	        if (dlg.chapter_start->isChecked()) chapter = 0;
	        else if (dlg.chapter_end->isChecked()) chapter = 1000;
	    }

	    if (frames == m_ruler->getSliderValue(0)) {
		// only comment has changed
		m_ruler->slotEditGuide(comment, chapter);
	    }
	    else {
		m_ruler->slotDeleteGuide();
		m_ruler->slotAddGuide(frames, dlg.marker_comment->text(), chapter);
	    	trackView()->invalidatePartialBackBuffer(pos - 2, pos + 2);
		trackView()->invalidatePartialBackBuffer(frames - 2, frames + 2);
	    }
	}
    }

    void KTimeLine::gotoGuide(int ix) {
	//move to guide number ix (substract 100 because in the popupmenu, we start the items id at 100)
	int pos = *(m_ruler->timelineGuides().at(ix - 100));
	m_ruler->setSliderValue(0, pos);
	ensureCursorVisible();	
    }

    void KTimeLine::clearGuides() {
	m_ruler->clearGuides();
	trackView()->invalidateBackBuffer();
    }

    void KTimeLine::drawSelection(QPoint start, QPoint end) {
	trackView()->drawSelection(start, end);
	trackView()->invalidateBackBuffer();
    }

    void KTimeLine::finishMultiSelection(QPoint start, QPoint end) {
	trackView()->drawSelection(QPoint(), QPoint());
	trackView()->invalidateBackBuffer();
    }


}				// namespace Gui
