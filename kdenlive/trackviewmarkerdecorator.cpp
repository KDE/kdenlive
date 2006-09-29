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
	m_markerPixmap = KGlobal::iconLoader()->loadIcon("kdenlive_marker", KIcon::Small, 15);
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

	painter.setClipRect(sx, rect.y(), ex, rect.height());

	QValueVector < GenTime > markers = clip->snapMarkersOnTrack();

	QValueVector < GenTime >::iterator itt = markers.begin();

	while (itt != markers.end()) {
	    int x =
		(int) timeline()->mapValueToLocal((*itt).
		frames(document()->framesPerSecond()));

	    if ((x >= sx - 7) && (x <= sx + ex + 7)) {
		QPen currentPen = painter.pen();
		QBrush currentBrush = painter.brush();

		painter.setPen(QColor(255, 0, 0));
		painter.setBrush(QColor(255, 0, 0));
		painter.drawLine(x, rect.y(), x, rect.y() + rect.height());

		painter.drawPixmap(x - 7, rect.y() + rect.height() / 2  - 7, m_markerPixmap);

		/*painter.setPen(Qt::black);
		painter.drawEllipse(x - (rect.height() / 4),
		    rect.y() + (rect.height() / 4),
		    rect.height() / 2, rect.height() / 2);*/

		painter.setPen(currentPen);
		painter.setBrush(currentBrush);
	    }

	    ++itt;
	}

	painter.setClipping(false);
    }

}				// namespace Gui
