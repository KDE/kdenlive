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

KMMTimeLineTrackView::KMMTimeLineTrackView(KMMTimeLine &timeLine, QWidget *parent, const char *name) :
						QWidget(parent, name),
						m_timeline(timeLine),
						m_trackBaseNum(-1),
						m_panelUnderMouse(0)
{
	// we draw everything ourselves, no need to draw background.
	setBackgroundMode(Qt::NoBackground);
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

	KMMTrackPanel *panel = m_timeline.trackList().first();
	while(panel != 0) {
		widgetHeight = panel->height();

	  QRect rect(0, totalHeight, width(), widgetHeight);
	  panel->drawToBackBuffer(painter, rect);

		totalHeight+=widgetHeight;
		panel = m_timeline.trackList().next();
	}

	repaint();
}

/** This event occurs when a mouse button is pressed. */
void KMMTimeLineTrackView::mousePressEvent(QMouseEvent *event)
{
	KMMTrackPanel *panel = panelAt(event->y());

	if(panel) {
	 	if(event->button() == LeftButton) {
			if(panel->mousePressed(event)) m_panelUnderMouse = panel;
		}
	}
}

/** This event occurs when a mouse button is released. */
void KMMTimeLineTrackView::mouseReleaseEvent(QMouseEvent *event)
{
	if(m_panelUnderMouse) {
		if(event->button() == LeftButton) {
			if(m_panelUnderMouse->mouseReleased(event)) m_panelUnderMouse = 0;
		}
	} else {
			m_timeline.selectNone();
	}
}

/** This event occurs when the mouse has been moved. */
void KMMTimeLineTrackView::mouseMoveEvent(QMouseEvent *event)
{
	if(m_panelUnderMouse) {
		if(event->state() & LeftButton) {
			if(!m_panelUnderMouse->mouseMoved(event)) {
				m_panelUnderMouse = 0;
			}
		} else {
			m_panelUnderMouse->mouseReleased(event);
			m_panelUnderMouse = 0;
		}
	} else {
		KMMTrackPanel *panel = panelAt(event->y());
		if(panel) {
			setCursor(panel->getMouseCursor(event));
		} else {
			setCursor(QCursor(Qt::ArrowCursor));
		}
	}
}

KMMTrackPanel *KMMTimeLineTrackView::panelAt(int y)
{
	int totalHeight = 0;
	int widgetHeight;
	KMMTrackPanel *panel = m_timeline.trackList().first();

	while(panel != 0) {
	  widgetHeight = panel->height();

	  if((totalHeight < y) && (totalHeight + widgetHeight > y)) break;

		totalHeight+=widgetHeight;
		panel = m_timeline.trackList().next();
	}

	return panel;
}
