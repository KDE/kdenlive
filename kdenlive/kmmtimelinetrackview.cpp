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

#include <iostream>
#include <math.h>

#include <qpainter.h>
#include <qcursor.h>
#include <qpopupmenu.h>

#include <kdebug.h>

#include "kdenlive.h"
#include "kmmtimelinetrackview.h"
#include "kmmtimeline.h"
#include "kmmtrackpanel.h"

KMMTimeLineTrackView::KMMTimeLineTrackView(KMMTimeLine &timeLine, KdenliveApp *app, QWidget *parent, const char *name) :
						QWidget(parent, name),
						m_timeline(timeLine),
						m_trackBaseNum(-1),
						m_panelUnderMouse(0),
						m_app(app)
{
	// we draw everything ourselves, no need to draw background.
	setBackgroundMode(Qt::NoBackground);
	setMouseTracking(true);
	m_bufferInvalid = false;
}

KMMTimeLineTrackView::~KMMTimeLineTrackView()
{
}

void KMMTimeLineTrackView::resizeEvent(QResizeEvent *event)
{
	m_backBuffer.resize(event->size().width(), event->size().height());
	drawBackBuffer();
}

void KMMTimeLineTrackView::paintEvent(QPaintEvent *event)
{
	if(m_bufferInvalid) {
		drawBackBuffer();
		m_bufferInvalid = false;
	}

	QPainter painter(this);

	painter.drawPixmap(event->rect().x(), event->rect().y(),
										m_backBuffer,
										event->rect().x(), event->rect().y(),
										event->rect().width(), event->rect().height());
}

void KMMTimeLineTrackView::drawBackBuffer()
{
	QPainter painter(&m_backBuffer);

	painter.fillRect(0, 0, width(), height(), palette().active().background());

	KMMTrackPanel *panel = m_timeline.trackList().first();
	while(panel != 0) {
    int y = panel->y()- this->y();
    
	  QRect rect(0, y, width(), panel->height());
	  panel->drawToBackBuffer(painter, rect);

		panel = m_timeline.trackList().next();
	}
}

/** This event occurs when a mouse button is pressed. */
void KMMTimeLineTrackView::mousePressEvent(QMouseEvent *event)
{
	if(event->button() == RightButton) {
		QPopupMenu *menu = (QPopupMenu *)m_app->factory()->container("timeline_context", m_app);
		if(menu) {
			menu->popup(QCursor::pos());
		}
	} else {
		KMMTrackPanel *panel = panelAt(event->y());
		if(m_panelUnderMouse != 0)
		{
			kdWarning() << "Error - mouse Press Event with panel already under mouse" << endl;
		}

		if(panel) {
	 		if(event->button() == LeftButton) {
				if(panel->mousePressed(event)) m_panelUnderMouse = panel;
			}
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
			m_timeline.addCommand(m_timeline.selectNone(), true);
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
	KMMTrackPanel *panel = m_timeline.trackList().first();

	while(panel != 0) {
    int sy = panel->y() - this->y();
    int ey = sy + panel->height();
    
	  if((y >= sy) && (y<ey)) break;

		panel = m_timeline.trackList().next();
	}

	return panel;
}

/** Invalidate the back buffer, alerting the trackview that it should redraw itself. */
void KMMTimeLineTrackView::invalidateBackBuffer()
{
	m_bufferInvalid = true;
	update();
}
