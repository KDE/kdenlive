/***************************************************************************
                          kmmtimelinetrackview.cpp  -  description
                             -------------------
    begin                : Wed Aug 7 2002
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

#include <qpainter.h>
#include <qcursor.h>

#include <kdebug.h>

#include "kmmtimelinetrackview.h"
#include "kmmtimeline.h"
#include "kmmtrackpanel.h"

int KMMTimeLineTrackView::resizeTolerance = 4;

KMMTimeLineTrackView::KMMTimeLineTrackView(KMMTimeLine &timeLine, QWidget *parent, const char *name) :
						QWidget(parent, name),
						m_timeLine(timeLine),
						m_trackBaseNum(-1),
						m_clipUnderMouse(0),
						m_panelUnderMouse(0)
{
	// we draw everything ourselves, no need to draw background.
	setBackgroundMode(Qt::NoBackground);
	m_resizeState = None;
	setMouseTracking(true);
}

KMMTimeLineTrackView::~KMMTimeLineTrackView()
{
}

void KMMTimeLineTrackView::resizeEvent(QResizeEvent *event)
{
	m_backBuffer.resize(width(), height());
	drawBackBuffer();
}

void KMMTimeLineTrackView::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);

	painter.drawPixmap(event->rect().x(), event->rect().y(),
										m_backBuffer,
										event->rect().x(), event->rect().y(),
										event->rect().width(), event->rect().height());
}

void KMMTimeLineTrackView::drawBackBuffer()
{
	int totalHeight = 0;
	int widgetHeight;

	QPainter painter(&m_backBuffer);

	painter.fillRect(0, 0, width(), height(), palette().active().background());	

	KMMTrackPanel *panel = m_timeLine.trackList().first();
	while(panel != 0) {
		widgetHeight = panel->height();

	  QRect rect(0, totalHeight, width(), widgetHeight);
	  panel->drawToBackBuffer(painter, rect);

		totalHeight+=widgetHeight;
		panel = m_timeLine.trackList().next();
	}

	repaint();
}

/** This event occurs when a mouse button is pressed. */
void KMMTimeLineTrackView::mousePressEvent(QMouseEvent *event)
{
	GenTime mouseTime(m_timeLine.mapLocalToValue(event->x()), 25);	


	KMMTrackPanel *panel = panelAt(event->y());

	if(panel) {		
		if(event->button() == LeftButton) {
			m_panelUnderMouse = panel;			
			m_clipUnderMouse = panel->docTrack().getClipAt(mouseTime);
		}
	}
}

/** This event occurs when a mouse button is released. */
void KMMTimeLineTrackView::mouseReleaseEvent(QMouseEvent *event)
{
	GenTime mouseTime(m_timeLine.mapLocalToValue(event->x()), 25);
	
	if(event->button() == LeftButton) {
		if(m_resizeState != None) {
		} else {
			if(m_clipUnderMouse) {			
				if (event->state() & ControlButton) {
				  m_timeLine.toggleSelectClipAt(m_panelUnderMouse->docTrack(),mouseTime);
				} else if(event->state() & ShiftButton) {
				  m_timeLine.selectClipAt(m_panelUnderMouse->docTrack(),mouseTime);
				} else {
					m_timeLine.selectNone();
				  m_timeLine.selectClipAt(m_panelUnderMouse->docTrack(),mouseTime);
				}
			} else {
				m_timeLine.selectNone();
			}
			drawBackBuffer();
		}

		m_clipUnderMouse = 0;
		m_panelUnderMouse = 0;
	}
}

/** This event occurs when the mouse has been moved. */
void KMMTimeLineTrackView::mouseMoveEvent(QMouseEvent *event)
{
	GenTime mouseTime(m_timeLine.mapLocalToValue(event->x()), 25);

	if((event->state() & LeftButton) != LeftButton) {
		KMMTrackPanel *panel = panelAt(event->y());

		if(panel) {
			DocClipBase *clip = panel->docTrack().getClipAt(mouseTime);
			if(clip) {
				if( fabs(m_timeLine.mapValueToLocal(clip->trackStart().frames(25)) - event->x()) < resizeTolerance) {
					m_resizeState = Start;
					setCursor(QCursor(Qt::SizeHorCursor));
					return;					
				}
				if( fabs(m_timeLine.mapValueToLocal((clip->trackEnd()).frames(25)) - event->x()) < resizeTolerance) {
					m_resizeState = End;					
					setCursor(QCursor(Qt::SizeHorCursor));
					return;					
				}
			}
		}
				
		setCursor(QCursor(Qt::ArrowCursor));
		m_resizeState = None;
		return;
	}

	if(m_clipUnderMouse) {
		if(m_resizeState != None) {
			if(m_resizeState == Start) {
				m_panelUnderMouse->docTrack().resizeClipTrackStart(m_clipUnderMouse, mouseTime);
				drawBackBuffer();
			} else if(m_resizeState == End) {
				m_panelUnderMouse->docTrack().resizeClipCropDuration(m_clipUnderMouse, mouseTime);
				drawBackBuffer();
			} else {
				kdError() << "Unknown resize state reached in KMMTimeLineTrackView::mouseMoveEvent()" << endl;
				kdError() << "(this message should never be seen!)" << endl;
			}
		} else {
			if(!m_timeLine.clipSelected(m_clipUnderMouse)) {
				if ((event->state() & ControlButton) || (event->state() & ShiftButton)) {
					m_timeLine.selectClipAt(m_panelUnderMouse->docTrack(),mouseTime);
				} else {
					m_timeLine.selectNone();
					m_timeLine.selectClipAt(m_panelUnderMouse->docTrack(),mouseTime);	
				}
			}								
			m_clipOffset = mouseTime - m_clipUnderMouse->trackStart();
			m_timeLine.initiateDrag(m_clipUnderMouse, m_clipOffset);
		}
	}
}

KMMTrackPanel *KMMTimeLineTrackView::panelAt(int y)
{
	int totalHeight = 0;
	int widgetHeight;
	KMMTrackPanel *panel = m_timeLine.trackList().first();

	while(panel != 0) {
	  widgetHeight = panel->height();

	  if((totalHeight < y) && (totalHeight + widgetHeight > y)) break;

		totalHeight+=widgetHeight;
		panel = m_timeLine.trackList().next();
	}

	return panel;
}
