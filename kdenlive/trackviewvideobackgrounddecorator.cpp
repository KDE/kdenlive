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
                                              const QColor & unselected, bool shift):DocTrackDecorator(timeline,
                                              doc), m_selected(selected), m_unselected(unselected),
    	m_shift(shift) {

    } 

    TrackViewVideoBackgroundDecorator::~TrackViewVideoBackgroundDecorator() {
            }

    // virtual
    void TrackViewVideoBackgroundDecorator::paintClip(double startX,
                    double endX, QPainter & painter, DocClipRef * clip, QRect & rect,
                    bool selected) {
							  int sx = (int)startX;
							  int ex = (int)endX;
			
                        if (sx < rect.x()) {
                            sx = rect.x();
                        }
                        if (ex > rect.x() + rect.width()) {
                            ex = rect.x() + rect.width();
                        }
                        int y = rect.y();
                        int h = rect.height();

                        QPixmap startThumbnail = clip->thumbnail();
			int drawWidth = startThumbnail.width();

                        if (m_shift && clip->audioChannels() > 0 && clip->speed() == 1.0) {
                            h = h / 3 * 2;
			    drawWidth = drawWidth / 3 * 2;
			    startThumbnail = startThumbnail.convertToImage().smoothScale(drawWidth, h);
			}
			ex -= sx;
			//kdDebug()<< "++++++++  VIDEO REFRESH ("<<clip->name()<<"): "<<startX<<", "<<endX<<", RECT: "<<rect.x()<<", "<<rect.width()<<endl;

                        QColor col = selected ? m_selected : m_unselected;
			// fill clip with color
			painter.setClipRect(sx, rect.y(), ex, h);
                        painter.fillRect(sx, rect.y(), ex, h, col);
			

                        double aspect = 4.0 / 3.0;
			//width of a frame shown in timeline
                        int width =
                                (int) timeline()->mapValueToLocal(1) -
                                (int) timeline()->mapValueToLocal(0);

								int width1 = (int)((h) * aspect);
                        if (width1 > width)
                            width = width1;
                        //int i = sx;
                        //int frame = 0;
								//int clipStart = (int)timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));
								//int clipEnd = (int)timeline()->mapValueToLocal(clip->trackEnd().frames(document()->framesPerSecond()));
                        /* Use the clip's default thumbnail & scale it to track size to decorate until we have some better stuff */
                        
                        if (endX - startX < drawWidth)
									drawWidth = (int)(endX - startX);
                        if (ex + sx > endX - startThumbnail.width()) 
			{
                            QPixmap endThumbnail = clip->thumbnail(true);
			    if (m_shift && clip->audioChannels() > 0 && clip->speed() == 1.0)
				endThumbnail = endThumbnail.convertToImage().smoothScale(drawWidth, h);
			    
				 painter.drawPixmap((int)(endX-drawWidth), y, endThumbnail, 0, 0, drawWidth, h);
                        }
								if (sx < startX + startThumbnail.width()) painter.drawPixmap((int)startX, y, startThumbnail, 0, 0, drawWidth, h);
                        //painter.drawRect(i, y, drawWidth, h);
	//}
        		painter.setClipping(false);
                    }

};
