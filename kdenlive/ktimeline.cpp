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

#include <klocale.h>
#include <qscrollbar.h>
#include <qscrollview.h>
#include <qhbox.h>
#include <qlabel.h>

#include <kdebug.h>

#include "krulertimemodel.h"
#include "ktimeline.h"
#include "ktrackview.h"
#include "kresizecommand.h"
#include "kscalableruler.h"
#include "kdenlive.h"

uint KTimeLine::snapTolerance = 10;

namespace
{
uint g_scrollTimerDelay = 50;
uint g_scrollThreshold = 50;
}

KTimeLine::KTimeLine( QWidget *rulerToolWidget, QWidget *scrollToolWidget, QWidget *parent, const char *name ) :
		QVBox( parent, name ),
		m_scrollTimer( this, "scroll timer" ),
		m_scrollingRight( true ),
		m_framesPerSecond(25),
		m_editMode("undefined"),
		m_panelWidth(200)
{
	m_rulerBox = new QHBox( this, "ruler box" );
	m_trackScroll = new QScrollView( this, "track view", WPaintClever );
	m_scrollBox = new QHBox( this, "scroll box" );

	m_rulerToolWidget = rulerToolWidget;
	if ( !m_rulerToolWidget ) m_rulerToolWidget = new QLabel( i18n( "Ruler" ), 0, "ruler" );
	m_rulerToolWidget->reparent( m_rulerBox, QPoint( 0, 0 ) );

	m_ruler = new KScalableRuler( new KRulerTimeModel(), m_rulerBox, name );
	m_ruler->addSlider( KRuler::TopMark, 0 );
	//added inpoint/outpoint markers -reh
	m_ruler->addSlider( KRuler::StartMark, 0 );
	m_ruler->addSlider( KRuler::EndMark, m_ruler->maxValue() );
	m_ruler->setAutoClickSlider( 0 );

	m_scrollToolWidget = scrollToolWidget;
	if ( !m_scrollToolWidget ) m_scrollToolWidget = new QLabel( i18n( "Scroll" ), 0, "Scroll" );
	m_scrollToolWidget->reparent( m_scrollBox, QPoint( 0, 0 ) );
	m_scrollBar = new QScrollBar( -100, 5000, 50, 500, 0, QScrollBar::Horizontal, m_scrollBox, "horizontal ScrollBar" );

	m_trackViewArea = new KTrackView( *this, m_trackScroll, "track view area" );

	m_trackScroll->enableClipper( TRUE );
	m_trackScroll->setVScrollBarMode( QScrollView::AlwaysOn );
	m_trackScroll->setHScrollBarMode( QScrollView::AlwaysOff );
	m_trackScroll->setDragAutoScroll( true );

	setPanelWidth(200);

	m_ruler->setValueScale( 1.0 );

	connect( m_scrollBar, SIGNAL( valueChanged( int ) ), m_ruler, SLOT( setStartPixel( int ) ) );
	connect( m_scrollBar, SIGNAL( valueChanged( int ) ), m_ruler, SLOT( repaint() ) );
	connect( m_scrollBar, SIGNAL( valueChanged( int ) ), this, SLOT( drawTrackViewBackBuffer() ) );

	connect( m_ruler, SIGNAL( scaleChanged( double ) ), this, SLOT( resetProjectSize() ) );
	connect( m_ruler, SIGNAL( sliderValueChanged( int, int ) ), m_trackViewArea, SLOT( invalidateBackBuffer() ) );
	connect( m_ruler, SIGNAL( sliderValueChanged( int, int ) ), m_ruler, SLOT( repaint() ) );
	connect( m_ruler, SIGNAL( sliderValueChanged( int, int ) ), this, SLOT( slotSliderMoved( int, int ) ) );

	connect( m_ruler, SIGNAL( requestScrollLeft() ), this, SLOT( slotScrollLeft() ) );
	connect( m_ruler, SIGNAL( requestScrollRight() ), this, SLOT( slotScrollRight() ) );
	connect( &m_scrollTimer, SIGNAL( timeout() ), this, SLOT( slotTimerScroll() ) );

	connect( m_trackViewArea, SIGNAL( rightButtonPressed() ), this, SIGNAL( rightButtonPressed() ) );
	
	setAcceptDrops( true );

	m_trackList.setAutoDelete( true );
}

KTimeLine::~KTimeLine()
{}

void KTimeLine::appendTrack( KTrackPanel *track )
{
	if(track) {
		insertTrack( m_trackList.count(), track );
	} else {
		kdWarning() << "KTimeLine::appendTrack() : Trying to append a NULL track!" << endl;
	}
}

/** Inserts a track at the position specified by index */
void KTimeLine::insertTrack( int index, KTrackPanel *track )
{
	assert(track);
	track->reparent( m_trackScroll->viewport(), 0, QPoint( 0, 0 ), TRUE );
	m_trackScroll->addChild( track );

	m_trackList.insert( index, track );

	resizeTracks();
}

void KTimeLine::resizeEvent( QResizeEvent *event )
{
	resizeTracks();
}

void KTimeLine::resizeTracks()
{
	int height = 0;
	int widgetHeight;

	QWidget *panel = m_trackList.first();

	while ( panel != 0 ) {
		widgetHeight = panel->height();

		m_trackScroll->moveChild( panel, 0, height );
		panel->resize( m_panelWidth, widgetHeight );

		height += widgetHeight;

		panel = m_trackList.next();
	}

	m_trackScroll->moveChild( m_trackViewArea, m_panelWidth, 0 );
	int newWidth = m_trackScroll->visibleWidth() - m_panelWidth ;
	if(newWidth < 0) newWidth = 0;
	m_trackViewArea->resize( newWidth, height );

	int viewWidth = m_trackScroll->visibleWidth() - m_panelWidth;
	if ( viewWidth < 1 ) viewWidth = 1;

	QPixmap pixmap( viewWidth , height );

	m_trackScroll->resizeContents( m_trackScroll->visibleWidth(), height );
}

/** No descriptions */
void KTimeLine::polish()
{
	resizeTracks();
}

/** This method maps a local coordinate value to the corresponding
value that should be represented at that position. By using this, there is no need to calculate scale factors yourself. Takes the x
coordinate, and returns the value associated with it. */
double KTimeLine::mapLocalToValue( const double coordinate ) const
{
	return m_ruler->mapLocalToValue( coordinate );
}

/** Takes the value that we wish to find the coordinate for, and returns the x coordinate. In cases where a single value covers multiple
pixels, the left-most pixel is returned. */
double KTimeLine::mapValueToLocal( const double value ) const
{
	return m_ruler->mapValueToLocal( value );
}

void KTimeLine::drawTrackViewBackBuffer()
{
	m_trackViewArea->invalidateBackBuffer();
}

/** Returns m_trackList

Warning - this method is a bit of a hack, not good OO practice, and should be removed at some point. */
QPtrList<KTrackPanel> &KTimeLine::trackList()
{
	return m_trackList;
}

void KTimeLine::scrollViewLeft()
{
	m_scrollBar->subtractLine();
}

void KTimeLine::scrollViewRight()
{
	m_scrollBar->addLine();
}

/** Sets a new time scale for the timeline. This in turn calls the correct kruler funtion and updates
the display. The scale is the size of one frame.*/
void KTimeLine::setTimeScale( double scale )
{
	int localValue = ( int ) mapValueToLocal( m_ruler->getSliderValue( 0 ) );

	m_ruler->setValueScale( scale );

	m_scrollBar->setValue( ( int ) ( scale * m_ruler->getSliderValue( 0 ) ) - localValue );

	drawTrackViewBackBuffer();
}

/** Calculates the size of the project, and sets up the timeline to accomodate it. */
void KTimeLine::slotSetProjectLength(const GenTime &size)
{
	int frames = size.frames( m_framesPerSecond );

	m_scrollBar->setRange( 0, ( frames * m_ruler->valueScale()) + m_scrollBar->width() );
	m_ruler->setRange( 0, frames );

	emit projectLengthChanged( frames );
}

void KTimeLine::resetProjectSize()
{
	m_scrollBar->setRange( 0, (m_ruler->maxValue() * m_ruler->valueScale()) + m_scrollBar->width() );
}

GenTime KTimeLine::seekPosition() const
{
	return GenTime( m_ruler->getSliderValue( 0 ), m_framesPerSecond );
}

void KTimeLine::setEditMode(const QString &editMode)
{
	m_editMode = editMode;
}

const QString &KTimeLine::editMode() const
{
	return m_editMode;
}

/** A ruler slider has moved - do something! */
void KTimeLine::slotSliderMoved( int slider, int value )
{
	if ( slider == 0 ) {
		emit seekPositionChanged( GenTime( value, m_framesPerSecond ) );
	}
}

/** Seek the timeline to the current position. */
void KTimeLine::seek( const GenTime &time )
{
	m_ruler->setSliderValue( 0, ( int ) floor( time.frames( m_framesPerSecond ) + 0.5 ) );
}

/** Returns the correct "time under mouse", taking into account whether or not snap to frame is on or off, and other relevant effects. */
GenTime KTimeLine::timeUnderMouse( double posX )
{
	GenTime value( m_ruler->mapLocalToValue( posX ), m_framesPerSecond );

	if ( snapToFrame() ) value = GenTime( floor( value.frames( m_framesPerSecond ) + 0.5 ), m_framesPerSecond );

	return value;
}

bool KTimeLine::snapToBorders() const
{
	return m_snapToBorder;
}

bool KTimeLine::snapToFrame() const
{
	return m_snapToFrame;
}

bool KTimeLine::snapToMarkers() const
{
	return m_snapToMarker;
}

bool KTimeLine::snapToSeekTime() const
{
#warning snapToSeekTime always returns true - needs to be wired up in KdenliveApp to
	#warning a check box.
	return true;
}

GenTime KTimeLine::projectLength() const
{
	return GenTime( m_ruler->maxValue(), m_framesPerSecond );
}

void KTimeLine::slotTimerScroll()
{
	if ( m_scrollingRight ) {
		m_scrollBar->addLine();
	} else {
		m_scrollBar->subtractLine();
	}
}

void KTimeLine::slotScrollLeft()
{
	m_scrollBar->subtractLine();
}

void KTimeLine::slotScrollRight()
{
	m_scrollBar->addLine();
}

void KTimeLine::clearTrackList()
{
	m_trackList.clear();
	resizeTracks();
}

uint KTimeLine::scrollThreshold() const
{
	return g_scrollThreshold;
}

uint KTimeLine::scrollTimerDelay() const
{
	return g_scrollTimerDelay;
}

void KTimeLine::checkScrolling(const QPoint &pos)
{
	if ( pos.x() < scrollThreshold() ) {
		if ( !m_scrollTimer.isActive() ) {
			m_scrollTimer.start( scrollTimerDelay(), false );
		}
		m_scrollingRight = false;
	} else if ( m_trackViewArea->width() - pos.x() < scrollThreshold() ) {
		if ( !m_scrollTimer.isActive() ) {
			m_scrollTimer.start( g_scrollTimerDelay, false );
		}
		m_scrollingRight = true;
	} else {
		m_scrollTimer.stop();
	}
}

void KTimeLine::stopScrollTimer()
{
	m_scrollTimer.stop();
}

void KTimeLine::slotSetFramesPerSecond(double fps)
{
	m_framesPerSecond = fps;
}

void KTimeLine::setSnapToFrame(bool snapToFrame)
{
	m_snapToFrame = snapToFrame;
}

void KTimeLine::setSnapToBorder(bool snapToBorder)
{
	m_snapToBorder = snapToBorder;
}

void KTimeLine::setSnapToMarker(bool snapToMarker)
{
	m_snapToMarker= snapToMarker;
}

void KTimeLine::invalidateBackBuffer()
{
	m_trackViewArea->invalidateBackBuffer();
}

int KTimeLine::viewWidth() const
{
	return m_trackViewArea->width();
}

void KTimeLine::setPanelWidth(int width)
{
	m_panelWidth = width;

	m_rulerToolWidget->setMinimumWidth( m_panelWidth );
	m_rulerToolWidget->setMaximumWidth( m_panelWidth );

	m_scrollToolWidget->setMinimumWidth( m_panelWidth );
	m_scrollToolWidget->setMaximumWidth( m_panelWidth );

	resizeTracks();
}

void KTimeLine::setInpointTimeline( const GenTime &inpoint )
{
	m_ruler->setSliderValue( 1, ( int ) floor( inpoint.frames( m_framesPerSecond ) ) );
}

void KTimeLine::setOutpointTimeline( const GenTime &outpoint )
{
	m_ruler->setSliderValue( 2, ( int ) floor( outpoint.frames( m_framesPerSecond ) ) );
}
