/***************************************************************************
                          trackviewdecorator  -  description
                             -------------------
    begin                : Thu Nov 27 2003
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
#ifndef TRACKVIEWDECORATOR_H
#define TRACKVIEWDECORATOR_H

#include <qobject.h>

class QPainter;
class QRect;

class DocClipRef;
class DocTrackBase;
class KdenliveDoc;

namespace Gui {
    class KTimeLine;

/**
View decorators implement the functionality required to display clips on the timeline.

@author Jason Wood
*/
    class TrackViewDecorator:public QObject {
      public:
	TrackViewDecorator(KTimeLine * timeline);

	virtual ~ TrackViewDecorator();

	/** This function will paint a clip on screen. This funtion must be provided by a derived class.
		@param startX - The x coordinate of the start of the clip. This is not necessarily in the painting rectangle.
		@param endX - The x coordinate of the end of the clip. This is not necessarily in the painting rectangle.
		@param painter - The QPainter that should be used to draw.
		@param clip - The clip that we should be drawing.
		@param rect - The bounding rectangle of the draw area.
		@param selected - True if the clip is selected, false otherwise.
	*/
	virtual void paintClip(double startX, double endX,
	    QPainter & painter, DocClipRef * clip, QRect & rect,
	    bool selected) = 0;

      protected:
	 KTimeLine * timeline() {
	    return m_timeline;
      } private:
	 KTimeLine * m_timeline;
    };

}				// namespace Gui

#endif
