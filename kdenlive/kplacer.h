/***************************************************************************
                          kplacer  -  description
                             -------------------
    begin                : Tue Apr 6 2004
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
#ifndef KPLACER_H
#define KPLACER_H

class QPainter;
class QRect;

namespace Gui {
    class TrackViewDecorator;

/**
A placer is a piece of code that "knows" where clips should be drawn on the timeline.

@author Jason Wood
*/
    class KPlacer {
      public:
	KPlacer();

	virtual ~ KPlacer();

	/**
	Paints the backbuffer into the relevant place using the painter supplied. The
	track should be drawn into the area provided in area
	*/
	virtual void drawToBackBuffer(QPainter & painter, QRect & rect,
	    TrackViewDecorator * decorator) = 0;

	/** Returns true if this track panel has a document track index. */
	virtual bool hasDocumentTrackIndex() const = 0;

	/** Returns the track index into the underlying document model used by this track. Returns -1 if this is inapplicable. */
	virtual int documentTrackIndex() const = 0;
    };

}				// namespace Gui
#endif
