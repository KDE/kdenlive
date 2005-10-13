/***************************************************************************
                          trackviewvideobackgrounddecorator  -  description
                             -------------------
    begin                : May 2005
    copyright            : (C) 2005 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef TRACKVIEWVIDEOBACKGROUNDDECORATOR_H
#define TRACKVIEWVIDEOBACKGROUNDDECORATOR_H

#include <doctrackdecorator.h>

#include <qcolor.h>

#include <ktimeline.h>

/**
Draws the base preview image for a clip; in correct acpect ration and stretch togiven height .

@author Marco Gittler
*/
namespace Gui{
class TrackViewVideoBackgroundDecorator : public DocTrackDecorator
{
public:
	TrackViewVideoBackgroundDecorator(KTimeLine* timeline,
									KdenliveDoc* doc,
									const QColor &selected,
									const QColor &unselected,
									int shift=-1);

	virtual ~TrackViewVideoBackgroundDecorator();

    virtual void paintClip(double startX, double endx, QPainter &painter, DocClipRef *clip, QRect &rect, bool selected) ;
private:
	
	QColor m_selected;
	QColor m_unselected;
	int m_shift;
};

};

#endif
