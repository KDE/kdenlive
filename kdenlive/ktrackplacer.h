/***************************************************************************
                         ktrackplacer  -  description
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
#ifndef KTRACKPLACER_H
#define KTRACKPLACER_H

#include "kplacer.h"

class KdenliveDoc;
class DocTrackBase;
class KTimeLine;

/**
A placer that draws clips on a track, as represented in a DocTrackBase.

@author Jason Wood
*/
class KTrackPlacer : public KPlacer
{
public:
	KTrackPlacer(KdenliveDoc *doc, KTimeLine *timeline, DocTrackBase *track);

	virtual ~KTrackPlacer();

	virtual void drawToBackBuffer(QPainter &painter, QRect &rect, TrackViewDecorator *decorator);

    	/** Returns true if this track panel has a document track index. */
    	virtual bool hasDocumentTrackIndex() const { return true; }

    	/** Returns the track index into the underlying document model used by this track. Returns -1 if this is inapplicable. */
    	virtual int documentTrackIndex()  const;
private:
	KdenliveDoc *m_document;
	DocTrackBase *m_docTrack;
	KTimeLine *m_timeline;
};

#endif
