/***************************************************************************
                          kclipplacer  -  description
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
#ifndef KCLIPPLACER_H
#define KCLIPPLACER_H

#include <kplacer.h>

class DocClipRef;

namespace Gui {
    class KTimeLine;

/**
A placer that draws a single clip on the track.

@author Jason Wood
*/
    class KClipPlacer:public KPlacer {
      public:
	KClipPlacer(KTimeLine * timeline, DocClipRef * clip);

	~KClipPlacer();

	virtual void drawToBackBuffer(QPainter & painter, QRect & rect,
	    TrackViewDecorator * decorator);

    /** Returns true if this track panel has a document track index. */
	virtual bool hasDocumentTrackIndex() const {
	    return false;
	}
    /** Returns the track index into the underlying document model used by this track. Returns -1 if this is inapplicable. */
	    virtual int documentTrackIndex() const;
      private:
	 DocClipRef * m_clip;
	KTimeLine *m_timeline;
    };

}				// namespace Gui
#endif
