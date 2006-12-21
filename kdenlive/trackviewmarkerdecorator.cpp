/***************************************************************************
                          trackviewmarkerdecorator  -  description
                             -------------------
    begin                : Fri Nov 28 2003
    copyright            : (C) 2003 by Jason Wood
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
#include "trackviewmarkerdecorator.h"

#include <qpainter.h>
#include <qrect.h>
#include <qtooltip.h>

#include <kiconloader.h>

#include "ktrackview.h"
#include "docclipref.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"

namespace Gui {

    TrackViewMarkerDecorator::TrackViewMarkerDecorator(KTimeLine *
	timeline, KdenliveDoc * doc, QWidget *parent):DocTrackDecorator(timeline, doc), m_parent(parent) 
    {
	m_markerUpPixmap = KGlobal::iconLoader()->loadIcon("kdenlive_markup", KIcon::Small, 9);
	m_markerDownPixmap = KGlobal::iconLoader()->loadIcon("kdenlive_markdown", KIcon::Small, 9);
    } 

    TrackViewMarkerDecorator::~TrackViewMarkerDecorator() {
    }


// virtual
    void TrackViewMarkerDecorator::paintClip(double startX, double endX,
	QPainter & painter, DocClipRef * clip, QRect & rect,
	bool selected) {
	int sx = (int)startX;
	int ex = (int)endX;

	if (sx < rect.x()) {
	    sx = rect.x();
	}
	if (ex > rect.x() + rect.width()) {
	    ex = rect.x() + rect.width();
	}
	ex -= sx;
	bool showMarkers = document()->showAllMarkers();
	if (!showMarkers)
	    painter.setClipRect(sx, rect.y(), ex, rect.height());

	QValueVector < CommentedTime > markers = clip->commentedTrackSnapMarkers();

	QValueVector < CommentedTime >::iterator itt = markers.begin();

	while (itt != markers.end()) {
	    int x =
		(int) timeline()->mapValueToLocal(((*itt).time()).
		frames(document()->framesPerSecond()));

	    if (((x >= sx - 7) && (x <= sx + ex + 7))  || showMarkers) {
		QPen currentPen = painter.pen();
		QBrush currentBrush = painter.brush();

		painter.setPen(Qt::black);
		//painter.drawLine(x, rect.y() + 6, x, rect.y() + rect.height() - 6);

		painter.drawPixmap(x - 4, rect.y() + 1, m_markerUpPixmap);
		painter.drawPixmap(x - 4, rect.y() + rect.height() - 6, m_markerDownPixmap);

		if (showMarkers) {
		    // Display all markers on timeline
		    QString txt = (*itt).comment();
		
		    QRect textBound = painter.boundingRect(0, 0, rect.width(), rect.height(), Qt::AlignLeft, txt);

		    painter.setBrush(Qt::yellow);
		    //painter.setPen(QPen(Qt::black, 0, Qt::DotLine));
		    painter.drawRect(x, rect.y() + 7, textBound.width() + 5, textBound.height());
		    painter.setPen(Qt::black);
	            painter.drawText(x, rect.y() + 7, textBound.width() + 5, textBound.height(), Qt::AlignCenter, txt);
		}

		painter.setPen(currentPen);
		painter.setBrush(currentBrush);
	    }

	    ++itt;
	}

	painter.setClipping(false);
    }

}				// namespace Gui
