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
#include "kdenlivesettings.h"

#include <qimage.h>
#include <iostream>

#include <mlt++/Mlt.h>


namespace Gui {
    TrackViewVideoBackgroundDecorator::
            TrackViewVideoBackgroundDecorator(KTimeLine * timeline,
                                              KdenliveDoc * doc, 
                                              const QColor & unselected, bool shift):DocTrackDecorator(timeline, doc),  m_unselected(unselected), m_shift(shift) {
	m_selected = m_unselected.light(140);
    } 

    TrackViewVideoBackgroundDecorator::~TrackViewVideoBackgroundDecorator() {
            }

    // virtual
    void TrackViewVideoBackgroundDecorator::paintClip(double startX,
                    double endX, QPainter & painter, DocClipRef * clip, QRect & rect,
                    bool selected) {
			int sx = (int)startX;
			int ex = (int)endX;
			if (sx < 0 ) sx = 0;
			if (ex <= 0) return;

                        if (sx < rect.x()) {
                            sx = rect.x();
                        }
                        if (ex > rect.x() + rect.width()) {
                            ex = rect.x() + rect.width();
                        }
                        int y = rect.y();
                        int h = rect.height();
			ex -= sx;
                        QColor col = selected ? m_selected : m_unselected;
			// fill clip with color
			painter.setClipRect(sx, y, ex, h);
                        painter.fillRect(sx, y, ex, h, col);
			

                        double aspect = 4.0 / 3.0;
			//width of a frame shown in timeline
                        int width =
                                (int) timeline()->mapValueToLocal(1) -
                                (int) timeline()->mapValueToLocal(0);

			int width1 = (int)((h) * aspect);
                        if (width1 > width) width = width1;

			if (timeline()->timeScale() != 100.0 || (clip->clipType() != DocClipBase::VIDEO && clip->clipType() != DocClipBase::AV )) {
                            /* Use the clip's default thumbnail & scale it to track size to decorate until we have some better stuff */
			    QPixmap startThumbnail = clip->thumbnail();
			    int drawWidth = startThumbnail.width();
                            if (endX - startX < drawWidth)
				drawWidth = (int)(endX - startX);
                            if (ex + sx > endX - startThumbnail.width()) 
			    {
                                QPixmap endThumbnail = clip->thumbnail(true);
			        painter.drawPixmap((int)(endX-drawWidth), y, endThumbnail, 0, 0, drawWidth, h);
                            }
			    if (sx < startX + startThumbnail.width()) 
			        painter.drawPixmap((int)startX, y, startThumbnail, 0, 0, drawWidth, h);
			}
			else {
			    /* Scale is 100%, draw a thumbnail for every frame */
			    int nbFrames = ex / width + 2;
			    int startFrame = timeline()->mapLocalToValue(sx);
			    int realStart = (int) timeline()->mapValueToLocal(startFrame);
			    // kdDebug()<<"///////STARTING AT FRAME ("<<clip->fileURL().path()<<"): "<<startFrame<<", "<<sx<<":"<<ex<<", "<<width<<endl;
			    startFrame = startFrame + (clip->cropStartTime() - clip->trackStart()).frames(document()->framesPerSecond());
			    // kdDebug()<<"///////Need to get "<<nbFrames<<" frames: "<<startFrame<<endl;
			    uint drawWidth = clip->thumbnail().width();
			    QPixmap image(width, h);
    			    char *tmp = KRender::decodedString(clip->fileURL().path());
    			    Mlt::Profile *profile = new Mlt::Profile( KdenliveSettings::videoprofile().ascii());
    			    Mlt::Producer m_producer(*profile, tmp);
    			    delete tmp;
			    if (m_producer.is_blank()) {
				kdDebug()<<"/*/*/*/*/ ERROR CANOT READ: "<<tmp<<endl;
				return;
			    }
			    m_producer.seek(startFrame);
			    Mlt::Filter m_convert(*profile, "avcolour_space");
    			    m_convert.set("forced", mlt_image_rgb24a);
    			    m_producer.attach(m_convert);
    			    mlt_image_format format = mlt_image_rgb24a;
			    painter.setPen(Qt::red);
			    for (int pos = startFrame; pos < startFrame + nbFrames; pos++) {
    				m_producer.seek(pos);
    				image.fill(Qt::black);
    				Mlt::Frame * frame = m_producer.get_frame();
    				if (frame) {
 			   	    uint8_t *thumb = frame->get_image(format, width, h);
    				    QImage tmpimage(thumb, width, h, 32, NULL, 0, QImage::IgnoreEndian);
				    tmpimage.setAlphaBuffer( true );
   			 	    if (!tmpimage.isNull()) bitBlt(&image, 0, 0, &tmpimage, 0, 0, width, h);
				    delete frame;
    				}
				else kdDebug()<<"// INVALID FRAME -------------"<<endl;
				int start = realStart + width * (pos - startFrame);
				painter.drawPixmap(start, y, image, 0, 0, width, h);
				painter.drawLine(start, y, start,  y + h);
			    }
			}
        		painter.setClipping(false);
                    }

};
