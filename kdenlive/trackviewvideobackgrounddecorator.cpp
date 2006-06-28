/***************************************************************************
                          trackviewvideobackgrounddecorator  -  description
                             -------------------
    begin                : May 2005
    copyright            : (C) 2005 Marco Gittler, g.marco@freenet.de
    copyright            : (C) 2006 Jean-Baptiste Mardelle, jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "trackviewvideobackgrounddecorator.h"

#include <qpainter.h>

#include "docclipref.h"
#include "gentime.h"
#include "kdenlivedoc.h"
#include "kdebug.h"
#include "ktimeline.h"
#include <qimage.h>
#include <iostream>


namespace Gui {
    TrackViewVideoBackgroundDecorator::
            TrackViewVideoBackgroundDecorator(KTimeLine * timeline,
                                              KdenliveDoc * doc, const QColor & selected,
                                              const QColor & unselected, int shift):DocTrackDecorator(timeline,
                                              doc), m_selected(selected), m_unselected(unselected),
    	m_shift(shift) {

    } 

    TrackViewVideoBackgroundDecorator::~TrackViewVideoBackgroundDecorator() {
            }

    // virtual
    void TrackViewVideoBackgroundDecorator::paintClip(double startX,
                    double endX, QPainter & painter, DocClipRef * clip, QRect & rect,
                    bool selected) {
                        int sx = startX;
                        int ex = endX;
			
                        if (sx < rect.x()) {
                            sx = rect.x();
                        }
                        if (ex > rect.x() + rect.width()) {
                            ex = rect.x() + rect.width();
                        }
                        int y = rect.y();
                        int h = rect.height();
                        if (m_shift)
                            h -= m_shift;

			//kdDebug()<< "++++++++  VIDEO REFRESH ("<<clip->name()<<"): "<<startX<<", "<<endX<<", RECT: "<<rect.x()<<", "<<rect.width()<<endl;

                        QColor col = selected ? m_selected : m_unselected;
			// fill clip with color
                        painter.fillRect(sx, rect.y(), ex-sx, rect.height(), col);
			painter.setClipRect(sx, rect.y(), ex, rect.height());

                        double aspect = 4.0 / 3.0;
			//width of a frame shown in timeline
                        int width =
                                (int) timeline()->mapValueToLocal(1) -
                                (int) timeline()->mapValueToLocal(0);

                        int width1 = (h) * aspect;
                        if (width1 > width)
                            width = width1;
                        int i = sx;
                        int frame = 0;
			int clipStart = timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));
			int clipEnd = timeline()->mapValueToLocal(clip->trackEnd().frames(document()->framesPerSecond()));
                        /* Use the clip's default thumbnail & scale it to track size to decorate until we have some better stuff */
                        QPixmap newimg = clip->thumbnail();
                        int drawWidth = newimg.width();
                        if (endX - startX < drawWidth)
                            drawWidth = endX - startX;
                        if (ex + sx > endX - newimg.width()) 
			{
                            painter.drawPixmap(endX-drawWidth, y, clip->thumbnail(true), 0, 0, drawWidth, h);
                        }
                        if (sx < startX + newimg.width()) painter.drawPixmap(startX, y, newimg, 0, 0, drawWidth, h);
                        //painter.drawRect(i, y, drawWidth, h);
	//}

                    }

};