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


class QPainter;
class QRect;

class DocClipRef;
class DocTrackBase;
class KTimeLine;
class KdenliveDoc;

/**
View decorators implement the functionality required to display clips on the timeline.

@author Jason Wood
*/
class TrackViewDecorator {
public:
    	TrackViewDecorator(KTimeLine *timeline);

    	virtual ~TrackViewDecorator();

	/**
	Paints the backbuffer into the relevant place using the painter supplied. The
	track should be drawn into the area provided in area
	*/
	virtual void drawToBackBuffer(QPainter &painter, QRect &rect) = 0;

protected:
	KTimeLine *timeline() { return m_timeline; }

private:
	KTimeLine *m_timeline;
};

#endif
