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
class KMMTimeLine;
class KdenliveDoc;

/**
View decorators implement the functionality required to display clips on the timeline.

@author Jason Wood
*/
class TrackViewDecorator{
public:
    TrackViewDecorator(KMMTimeLine *timeline, KdenliveDoc *doc, DocTrackBase *track);

    virtual ~TrackViewDecorator();


  	/** This function will paint a clip on screen. This funtion must be provided by a derived class. */
	virtual void paintClip(QPainter &painter, DocClipRef *clip, QRect &rect, bool selected) = 0;

	/**
	Paints the backbuffer into the relevant place using the painter supplied. The
	track should be drawn into the area provided in area
	*/
	void drawToBackBuffer(QPainter &painter, QRect &rect);

protected:
	KMMTimeLine *timeline() { return m_timeline; }
	KdenliveDoc *document() { return m_document; }
	DocTrackBase *docTrack() { return m_docTrack; }

private:
	KMMTimeLine *m_timeline;
	KdenliveDoc *m_document;
	DocTrackBase *m_docTrack;
};

#endif
