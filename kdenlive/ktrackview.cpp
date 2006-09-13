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
#include <qtooltip.h>

#include <kdebug.h>
#include <kmessagebox.h>

#include "ktrackview.h"
#include "ktimeline.h"
#include "kmmtrackpanel.h"
#include "kselectclipcommand.h"
#include "trackpanelfunction.h"

namespace Gui {

    KTrackView::KTrackView(KTimeLine & timeLine, QWidget * parent,
	const char *name):QWidget(parent, name), m_timeline(timeLine),
	m_trackBaseNum(-1), m_panelUnderMouse(0), m_function(0),
	m_dragFunction(0) {
	// we draw everything ourselves, no need to draw background.
	setBackgroundMode(Qt::NoBackground);
	setMouseTracking(true);
	m_bufferInvalid = false;

	setAcceptDrops(true);
	trackview_tips = new DynamicToolTip(this);
    } 

    KTrackView::~KTrackView() {
	delete trackview_tips;
    }

    void KTrackView::tip(const QPoint &pos, QRect &rect, QString &tipText) {
	KTrackPanel *panel = panelAt(pos.y());
    	KMMTrackPanel *m_panel = static_cast<KMMTrackPanel*> (panel);
	if (m_panel->trackType() == KEYFRAMETRACK) return;
	DocClipRef *underMouse = m_panel->getClipAt(pos.x());
	if (!underMouse) return;

    	QValueVector < CommentedTime > markers = underMouse->commentedTrackSnapMarkers();
    	QValueVector < CommentedTime >::iterator itt = markers.begin();
	int revativeOffset = m_panel->y() - y();
	int trackHeight = m_panel->height();
	while (itt != markers.end()) {
	    int x = m_panel->getLocalValue((*itt).time());
	    if ( fabs(x - pos.x()) < 5) {
	    	rect.setRect(x -7, revativeOffset + trackHeight/2 - 10, 15, 20);
		tipText = (*itt).comment();
		return;
	    }
	    ++itt;
    	}

	rect.setRect(m_panel->getLocalValue(underMouse->trackStart()), revativeOffset, abs(m_panel->getLocalValue(underMouse->duration()) - m_panel->getLocalValue(GenTime(0))), 20);
	tipText = underMouse->description();
	if (tipText.isEmpty()) tipText = underMouse->name();
    }

    void KTrackView::resizeEvent(QResizeEvent * event) {
        m_bufferDrawList.setFullRange(0, width());
	m_backBuffer.resize(event->size().width(), event->size().height());
	drawBackBuffer();
    }

    void KTrackView::wheelEvent ( QWheelEvent * e ) {
	if (KdenliveSettings::horizontalmouse()) {
        if ( ( e->state() & AltButton) == AltButton) {
            if ( e->orientation() == Horizontal) {
                if (e->delta() < 0) m_timeline.slotScrollDown();
                else m_timeline.slotScrollUp();
                e->accept();
            }
        }
        else if (( e->state() & ControlButton) == ControlButton) {
            if (e->delta() > 0) emit changeZoom(false);
            else emit changeZoom(true);
            e->accept();
        }
        else if ( e->orientation() == Vertical)
	{
		if (e->delta() < 0) m_timeline.slotScrollRight();
                else m_timeline.slotScrollLeft();
                e->accept();
	}
	}
	else {
	    if ( ( e->state() & AltButton) == AltButton) {
            if ( e->orientation() == Horizontal) {
                if (e->delta() < 0) m_timeline.slotScrollRight();
                else m_timeline.slotScrollLeft();
                e->accept();
            }
        }
        else if (( e->state() & ControlButton) == ControlButton) {
            if (e->delta() > 0) emit changeZoom(false);
            else emit changeZoom(true);
            e->accept();
        }
	else e->ignore();
	}
    }
    
    void KTrackView::paintEvent(QPaintEvent * event) {
        RangeListIterator < int >itt(m_bufferDrawList);
        
        while (!itt.finished()) {
            drawBackBuffer(itt.start(), itt.end());
            ++itt;
        }
        m_bufferDrawList.clear();
        
	/*if (m_bufferInvalid) {
	    drawBackBuffer();
	    m_bufferInvalid = false;
        }*/

        QPainter painter(this);
	painter.drawPixmap(event->rect().x(), event->rect().y(),
	    m_backBuffer,
	    event->rect().x(), event->rect().y(),
        event->rect().width(), event->rect().height());
    }

    void KTrackView::drawBackBuffer() {
	QPainter painter(&m_backBuffer);

	painter.fillRect(0, 0, width(), height(),
	    palette().active().background());

	KTrackPanel *panel = m_timeline.trackList().first();
	while (panel != 0) {
	    int y = panel->y() - this->y();
	    QRect rect(0, y, width(), panel->height());
	    if (panel->trackType() == KEYFRAMETRACK) // white background on transition track
		painter.fillRect(rect, palette().active().light());
	    else {
		painter.setPen(QColor(Qt::gray));
		painter.drawLine(0, y, width(), y); // gray line between the tracks
		painter.setPen(QColor(Qt::black));
	    }
	    panel->drawToBackBuffer(painter, rect);
	    panel = m_timeline.trackList().next();
	}

	// Draw guides
	QValueList < int > guides;
	guides = m_timeline.timelineGuides();
	QValueList < int >::Iterator it = guides.begin();
        for ( it = guides.begin(); it != guides.end(); ++it ) {
	int guidePosition = m_timeline.mapValueToLocal(*it);
	if (guidePosition >= 0 && guidePosition <= width()) {
	    painter.setPen(QColor(Qt::gray));
	    painter.drawLine(guidePosition, 0, guidePosition, height());
	    painter.setPen(QColor(Qt::black));
	}
        }

	// draw the vertical time marker
	int value = m_timeline.mapValueToLocal(m_timeline.localSeekPosition());
	painter.drawLine(value, 0, value, height());

	
    }
    
    void KTrackView::drawBackBuffer(int start, int end) {
        int sx = start - 2;
        int ex = end + 4;
        QPainter painter(&m_backBuffer);
        painter.fillRect(sx, 0, ex - sx, height(),
                         palette().active().background());
	//painter.setClipRect(sx, 0, ex - sx, height());

        KTrackPanel *panel = m_timeline.trackList().first();
        while (panel != 0) {
            int y = panel->y() - this->y();
	    QRect rect(sx, y, ex - sx, panel->height());
	    if (panel->trackType() == KEYFRAMETRACK) // white background on transition track
		painter.fillRect(rect, palette().active().light());
	    else {
		painter.setPen(QColor(Qt::gray));
		painter.drawLine(sx, y, ex, y); // gray line between the tracks
		painter.setPen(QColor(Qt::black));
	    }
            panel->drawToBackBuffer(painter, rect);
            panel = m_timeline.trackList().next();
        }

	// Draw guides
	QValueList < int > guides;
	guides = m_timeline.timelineGuides();
	QValueList < int >::Iterator it = guides.begin();
        for ( it = guides.begin(); it != guides.end(); ++it ) {
	int guidePosition = m_timeline.mapValueToLocal( *it );
	if (guidePosition >= start && guidePosition <= end) {
	    painter.setPen(QColor(Qt::gray));
	    painter.drawLine(guidePosition, 0, guidePosition, height());
	    painter.setPen(QColor(Qt::black));
	}
        }

	// draw the vertical time marker
	int value = m_timeline.mapValueToLocal(m_timeline.localSeekPosition());
	if (value >= start && value <= end) {
	    painter.drawLine(value, 0, value, height());
	}

    }


    KTrackPanel *KTrackView::panelAt(int y) {
	KTrackPanel *panel = m_timeline.trackList().first();

	while (panel != 0) {
	    int sy = panel->y() - this->y();
	    int ey = sy + panel->height();

	    if ((y >= sy) && (y < ey))
		break;

	    panel = m_timeline.trackList().next();
	}

	return panel;
    }

/** Invalidate the back buffer, alerting the trackview that it should redraw itself. */
    void KTrackView::invalidateBackBuffer() {
        m_bufferDrawList.addRange(0, width());
	m_bufferInvalid = true;
	update();
    }

void KTrackView::invalidateBackBuffer(int pos1, int pos2) {
   pos1 = (int) m_timeline.mapValueToLocal(pos1);
   pos2 = (int) m_timeline.mapValueToLocal(pos2);
   if (pos1 < pos2) {
      m_bufferDrawList.addRange(pos1 - 2, pos2 + 2);
      // Optimise painting and redraw only around the moving cursor
       update(pos1 - 2, -1, pos2 - pos1 + 4, height() + 1);
   }
   else {
      m_bufferDrawList.addRange(pos2 - 3, pos1 + 3);
      // Optimise painting and redraw only around the moving cursor
       update(pos2 - 2, -1, pos1 - pos2 + 4, height() + 1);
   }
}

    void KTrackView::registerFunction(const QString & name,
	TrackPanelFunction * function) {
	m_factory.registerFunction(name, function);
    }

    void KTrackView::clearFunctions() {
	m_factory.clearFactory();
    }

/** This event occurs when a double click occurs. */
void KTrackView::mouseDoubleClickEvent(QMouseEvent * event) {

KTrackPanel *panel = panelAt(event->y());
	    if (m_panelUnderMouse != 0) {
		kdWarning() <<
		    "Error - mouse Press Event with panel already under mouse"
		    << endl;
	    }
	    if (panel) {
		if (event->button() == LeftButton) {
		    bool result = false;
		    m_function =
			getApplicableFunction(panel, m_timeline.editMode(),
			event);
		    if (m_function)
			result = m_function->mouseDoubleClicked(panel, event);
		    if (result) {
			m_panelUnderMouse = panel;
		    } else {
			m_function = 0;
		    }
		}
	    }


//KMessageBox::sorry(this, "Hello");
}

/** This event occurs when a mouse button is pressed. */
    void KTrackView::mousePressEvent(QMouseEvent * event) {
        if (event->button() == RightButton) {
            emit rightButtonPressed();
            return;
        }
        KTrackPanel *panel = panelAt(event->y());
        if (m_panelUnderMouse != 0) {
            kdWarning() << "Error - mouse Press Event with panel already under mouse"<< endl;
        }
	if (panel) {
            if (event->button() == LeftButton) {
                bool result = false;
                m_function = getApplicableFunction(panel, m_timeline.editMode(), event);
                if (m_function)
                    result = m_function->mousePressed(panel, event);
                if (result) {
                    m_panelUnderMouse = panel;
                } else {
                    m_function = 0;
                }
            }
        }
    }

/** This event occurs when a mouse button is released. */
    void KTrackView::mouseReleaseEvent(QMouseEvent * event) {
	if (m_panelUnderMouse) {
	    if (event->button() == LeftButton) {
		bool result = false;
		if (m_function) {
		    result =
			m_function->mouseReleased(m_panelUnderMouse,
			event);
		    m_function = 0;
		}

		if (result)
		    m_panelUnderMouse = 0;
	    }
	}
    }

/** This event occurs when the mouse has been moved. */
    void KTrackView::mouseMoveEvent(QMouseEvent * event) {
	if (m_panelUnderMouse) {
	    if (event->state() & LeftButton) {
		bool result = false;
		if (m_function)
		    result =
			m_function->mouseMoved(m_panelUnderMouse, event);
		if (!result) {
		    m_panelUnderMouse = 0;
		    m_function = 0;
		}
	    } else {
		if (m_function) {
		    m_function->mouseReleased(m_panelUnderMouse, event);
		    m_function = 0;
		}
		m_panelUnderMouse = 0;
	    }
	} else {
	    KTrackPanel *panel = panelAt(event->y());
	    if (panel) {
		QCursor result(Qt::ArrowCursor);

		TrackPanelFunction *function =
		    getApplicableFunction(panel, m_timeline.editMode(),
		    event);
		if (function)
		    result = function->getMouseCursor(panel, event);

		setCursor(result);
	    } else {
		setCursor(QCursor(Qt::ArrowCursor));
	    }
	}
    }

// virtual
    void KTrackView::dragEnterEvent(QDragEnterEvent * event) {
	kdWarning() << "Drag enter event" << endl;

	// If there is a "panelUnderMouse" if means that the drag was initiated by one of the panels. Otherwise, the drag has reached
	// the timeline from somewhere else.
	if (m_panelUnderMouse) {
	    if ((!m_dragFunction) || (!m_dragFunction->dragEntered(m_panelUnderMouse, event))) {
		m_panelUnderMouse = 0;
	    }
	} else {
	    KTrackPanel *panel = panelAt(mapFromGlobal(QCursor::pos()).y());	//m_timeline.trackList().first();
	    if (panel) {
		if ((m_dragFunction)
		    && (m_dragFunction->dragEntered(panel, event))) {
		    m_panelUnderMouse = panel;
		}
	    }
	}
    }

// virtual
    void KTrackView::dragMoveEvent(QDragMoveEvent * event) {
	if (m_panelUnderMouse) {
	    if ((!m_dragFunction)
		|| (!m_dragFunction->dragMoved(m_panelUnderMouse,
			event))) {
		m_panelUnderMouse = 0;
	    }
	}
    }

// virtual
    void KTrackView::dragLeaveEvent(QDragLeaveEvent * event) {
        /*
	if (m_panelUnderMouse) {
	    if (m_dragFunction)
		m_dragFunction->dragLeft(m_panelUnderMouse, event);
	    m_panelUnderMouse = 0;
    }*/
    }

// virtual
    void KTrackView::dropEvent(QDropEvent * event) {
	if (m_panelUnderMouse) {
	    if ((!m_dragFunction) || (!m_dragFunction->dragDropped(m_panelUnderMouse, event))) {
		m_panelUnderMouse = 0;
	    }
	}
    }

    TrackPanelFunction *KTrackView::getApplicableFunction(KTrackPanel *
	panel, const QString & editMode, QMouseEvent * event) {
	TrackPanelFunction *function = 0;

	QStringList list = panel->applicableFunctions(editMode);
	QStringList::iterator itt = list.begin();

	while (itt != list.end()) {
	    TrackPanelFunction *testFunction = m_factory.function(*itt);
	    if (testFunction) {
		if (testFunction->mouseApplies(panel, event)) {
		    function = testFunction;
		    break;
		}
	    }

	    ++itt;
	}

	return function;
    }

    void KTrackView::setDragFunction(const QString & name) {
	m_dragFunction = m_factory.function(name);
    }


/**
bool KTrackPanel::mouseMoved( QMouseEvent *event )
{
	bool result = false;

	if ( m_function ) result = m_function->mouseMoved( event );

	if ( !result ) m_function = 0;

	return result;
}

QCursor KTrackPanel::getMouseCursor( QMouseEvent *event )
{


	return result;
}

// virtual
bool KTrackPanel::dragLeft ( QDragLeaveEvent *event )
{
	bool result = false;

	if(m_dragFunction) result = m_dragFunction->dragLeft(event);

	return result;
}

// virtual
bool KTrackPanel::dragDropped ( QDropEvent *event )
{
	bool result = false;

	if(m_dragFunction) result = m_dragFunction->dragDropped(event);

	return result;
}
*/

}				// namespace Gui
