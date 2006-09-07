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
#ifndef TRACKVIEWMARKERDECORATOR_H
#define TRACKVIEWMARKERDECORATOR_H

#include <qpixmap.h>
#include <doctrackdecorator.h>

namespace Gui {

    class KTimeLine;

/**
A TrackViewDecorator that displays snap markers on a clip.

@author Jason Wood
*/
    class TrackViewMarkerDecorator:public DocTrackDecorator {
      public:
	TrackViewMarkerDecorator(KTimeLine * timeline, KdenliveDoc * doc, QWidget *parent);

	virtual ~ TrackViewMarkerDecorator();

	virtual void paintClip(double startX, double endX,
	    QPainter & painter, DocClipRef * clip, QRect & rect,
	    bool selected);

    private:
	QPixmap m_markerPixmap;
	QWidget *m_parent;
    };

}				// namespace Gui
#endif
