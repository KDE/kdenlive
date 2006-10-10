/***************************************************************************
                          trackviewtransitiondecorator  -  description
                             -------------------
    begin                : Thu Jan 26 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
    
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include  <qpainter.h>
#include  <kdebug.h>

#include "trackviewtransitiondecorator.h"
#include "ktimeline.h"
#include "docclipref.h"
#include "docclipproject.h"
#include "gentime.h"
#include "kdenlivedoc.h"

namespace Gui {

    TrackViewTransitionDecorator::TrackViewTransitionDecorator(KTimeLine * timeline, KdenliveDoc * doc)
    :DocTrackDecorator(timeline, doc), m_effectName(""), m_effectIndex(0), m_paramName("") {
    } 
    
    TrackViewTransitionDecorator::~TrackViewTransitionDecorator() {
    }

// virtual
    void TrackViewTransitionDecorator::paintClip(double startX,
	double endX, QPainter & painter, DocClipRef * clip, QRect & rect,
	bool selected) {

	int sx = (int) startX;
	int ex = (int) endX;

	if(sx < rect.x()) {
	   sx = rect.x();
	   }
	if(ex > rect.x() + rect.width()) {
	   ex = rect.x() + rect.width();
	}

	ex -= sx;

	int ey = rect.height();
	int sy = rect.y() + ey;



	// draw outline box
//      painter.fillRect( sx, rect.y(), ex, rect.height(), col);

        TransitionStack m_transitions = clip->clipTransitions();

        if (m_transitions.isEmpty()) return;

        TransitionStack::iterator itt = m_transitions.begin();
	QFont orig = painter.font();
	QFont ft = orig;
	ft.setPixelSize(10);
	painter.setFont(ft);

        while (itt) {
	/*if (!transition) {
	    kdDebug() << "////// ERROR, EFFECT NOT FOUND" << endl;
	    return;
        }*/

        //uint half = rect.height()*2/3;
            QColor col(252,255,79);
            if (document()->application()->transitionPanel()->isActiveTransition( *itt)) col.setRgb(255,50,50);
            uint start = timeline()->mapValueToLocal((*itt)->transitionStartTime().frames(document()->framesPerSecond()));
            uint end = timeline()->mapValueToLocal((*itt)->transitionEndTime().frames(document()->framesPerSecond()))-start;
        //painter.fillRect(start, rect.y(), end, rect.height(), QBrush(col));  //, Qt::Dense5Pattern));
	QBrush br = painter.brush();
	painter.setBrush(QBrush(col));
        painter.drawRect(start, rect.y(), end, rect.height());
	painter.setBrush(br);
	painter.setClipRect(start, rect.y(), end, rect.height());
	
	 
	    
		// Draw transition track number if different than automatic
	    int trackNum = (*itt)->transitionTrack();
	    if (trackNum > 0) {
		QString txt;
		if (trackNum == 1) txt = i18n("> Background");
		else txt = QString("> %1").arg(trackNum - 1);
	        int textWidth = painter.fontMetrics().width( txt );
	        painter.setPen(Qt::darkRed);
	        painter.drawText((int) start + 20, rect.y(), textWidth + 8, rect.height(), Qt::AlignCenter | Qt::AlignHCenter, txt);
	        painter.setPen(Qt::black);
	    }

	    // draw transition icon
	    painter.drawPixmap((int) start + 3, rect.y() + (rect.height() - 15 ) / 2, (*itt)->transitionPixmap());
	    painter.setClipping(false);
	

        /*QPoint p1, p2;
        if ((*itt)->transitionStartTrack() == clip->trackNum()) {
            p1 = QPoint(start,rect.y());
            p2 = QPoint(start+end,rect.y()+rect.height()-1);
        }
        else {
            p1 = QPoint(start,rect.y()+rect.height()-1);
            p2 = QPoint(start+end,rect.y());
        }
        painter.drawLine(p1,p2);*/
        ++itt;
    }
    painter.setFont(orig);

        }

}				// namespace Gui
