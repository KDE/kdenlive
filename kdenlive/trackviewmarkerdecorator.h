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

#include <trackviewdecorator.h>

/**
A TrackViewDecorator that displays snap markers on a clip.

@author Jason Wood
*/
class TrackViewMarkerDecorator : public TrackViewDecorator
{
public:
    TrackViewMarkerDecorator(KMMTimeLine* timeline, 
    			KdenliveDoc* doc, 
    			DocTrackBase* track);

    virtual ~TrackViewMarkerDecorator();

    virtual void paintClip(QPainter& painter, DocClipRef* clip, QRect& rect, bool selected);

};

#endif
