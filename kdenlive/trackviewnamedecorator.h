/***************************************************************************
                          trackviewnamedecorator  -  description
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
#ifndef TRACKVIEWNAMEDECORATOR_H
#define TRACKVIEWNAMEDECORATOR_H

#include <trackviewdecorator.h>

/**
A decorator that adds the name of the clip to the clip itself.

@author Jason Wood
*/
class TrackViewNameDecorator : public TrackViewDecorator
{
public:
    TrackViewNameDecorator(KMMTimeLine* timeline, 
    			KdenliveDoc* doc, 
    			DocTrackBase* track);

    virtual ~TrackViewNameDecorator();

    virtual void paintClip(QPainter& painter, DocClipRef* clip, QRect& rect, bool selected);

};

#endif
