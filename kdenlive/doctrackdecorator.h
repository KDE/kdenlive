/***************************************************************************
                          doctrackdecorator  -  description
                             -------------------
    begin                : Wed Jan 7 2004
    copyright            : (C) 2004 by Jason Wood
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
#ifndef DOCTRACKDECORATOR_H
#define DOCTRACKDECORATOR_H

#include <trackviewdecorator.h>

/**
An abstract class for all decorators that implement a view based on a DocTrack.

@author Jason Wood
*/
class DocTrackDecorator : public TrackViewDecorator
{
public:
	DocTrackDecorator(KTimeLine *timeline, KdenliveDoc *doc, DocTrackBase *track);

    	~DocTrackDecorator();

  	/** This function will paint a clip on screen. This funtion must be provided by a derived class. */
	virtual void paintClip(QPainter &painter, DocClipRef *clip, QRect &rect, bool selected) = 0;


	/**
	Paints the backbuffer into the relevant place using the painter supplied. The
	track should be drawn into the area provided in area
	*/
	void drawToBackBuffer(QPainter &painter, QRect &rect);

protected:
	KdenliveDoc *document() { return m_document; }
	DocTrackBase *docTrack() { return m_docTrack; }
private:
	KdenliveDoc *m_document;
	DocTrackBase *m_docTrack;
};

#endif
