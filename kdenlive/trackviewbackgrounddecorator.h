/***************************************************************************
                          trackviewbackgrounddecorator  -  description
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
#ifndef TRACKVIEWBACKGROUNDDECORATOR_H
#define TRACKVIEWBACKGROUNDDECORATOR_H

#include <doctrackdecorator.h>

#include <qcolor.h>

#include <ktimeline.h>


namespace Gui {

/**
Draws the base image for a clip; draws a border around the clip, and fills it with a flat-shaded colour.

@author Jason Wood
*/
    class TrackViewBackgroundDecorator:public DocTrackDecorator {
      public:
	TrackViewBackgroundDecorator(KTimeLine * timeline,
	    KdenliveDoc * doc, const QColor & unselected);

	 virtual ~ TrackViewBackgroundDecorator();

	virtual void paintClip(double startX, double endx,
	    QPainter & painter, DocClipRef * clip, QRect & rect,
	    bool selected);
      private:
	 QColor m_selected;
	QColor m_unselected;
    };

}				// namespace Gui
#endif
