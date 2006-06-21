/***************************************************************************
                          trackviewbackgrounddecorator  -  description
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
#include "trackviewaudiobackgrounddecorator.h"

#include <qpainter.h>


#include "docclipref.h"
#include "gentime.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"
#include <qimage.h>
//only for testing please remove if video get drawn
#include <kstandarddirs.h>
namespace Gui {
    TrackViewAudioBackgroundDecorator::
	TrackViewAudioBackgroundDecorator(KTimeLine * timeline,
	KdenliveDoc * doc, const QColor & selected,
	const QColor & unselected, int size):DocTrackDecorator(timeline,
	doc), m_selected(selected), m_unselected(unselected),
	m_height(size) {
	connect(document()->renderer(),
	    SIGNAL(replyGetSoundSamples(const KURL &, int, int, double,
		    const QByteArray &, int, int, int, int, QPainter &)),
	    this, SLOT(setSoundSamples(const KURL &, int, int, double,
		    const QByteArray &, int, int, int, int, QPainter &)));
	 connect(this, SIGNAL(getSoundSamples(const KURL &, int, int,
		    double, int, int, int, int, int, QPainter &)),
	    document()->renderer(), SLOT(getSoundSamples(const KURL &, int,
		    int, double, int, int, int, int, int, QPainter &)));
    } TrackViewAudioBackgroundDecorator::
	~TrackViewAudioBackgroundDecorator() {
    }

// virtual
    void TrackViewAudioBackgroundDecorator::paintClip(double startX,
	double endX, QPainter & painter, DocClipRef * clip, QRect & rect,
	bool selected) {
	int sx = startX;	// (int)timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));
	int ex = endX;		//(int)timeline()->mapValueToLocal(clip->trackEnd().frames(document()->framesPerSecond()));

	if (sx < rect.x()) {
	    sx = rect.x();
	}
	if (ex > rect.x() + rect.width()) {
	    ex = rect.x() + rect.width();
	}
	//ex -= sx;
	int y = rect.y();
	int h = rect.height();

	if (m_height > 0) {
	    y += (h - m_height);
	    h = m_height;
	}

	QColor col = selected ? m_selected : m_unselected;
	double aspect = 4.0 / 3.0;
	int width=30;
	double FramesInOnePixel =
			width/(timeline()->mapValueToLocal(1) -
			timeline()->mapValueToLocal(0));
	//int width1 = (int) (h) * aspect;
	//if (width1 > width)
	//    width = width1;
	//kdDebug() << timeline()->mapLocalToValue(1) << " " << timeline()->mapLocalToValue(0) << " " << width << endl;
	double i = sx;

	int channels = 2;
	int frame = 0;
	painter.drawRect(sx, y, ex - sx, h);
	painter.fillRect(sx, y, ex - sx, h, col);
        
	for (; i < ex; i += width) {
	    if (i + width < rect.x() || i > rect.x() + rect.width())
		continue;
	    int deltaHeight = h / channels;
	    for (int countChannel = 0; countChannel < channels;
		countChannel++) {
			//clip->referencedClip()	
			if(clip->referencedClip()	)	{
				double FrameDiffFromNull=
					clip->trackStart().frames(document()->framesPerSecond());
				double RealFrame=
					timeline()->mapLocalToValue(i);
				//kdDebug() << "fromnull=" << RealFrame<< endl;
				
				QByteArray a=clip->getAudioThumbs(countChannel,
						RealFrame-FrameDiffFromNull,
						//timeline()->mapLocalToValue(i),
						/*(double)width**/FramesInOnePixel, width);
				//, i, y + deltaHeight * countChannel,h / channels, ex);
				drawChannel(countChannel,&a,i,y + deltaHeight * countChannel,h / channels, ex,painter);
				//emit(getSoundSamples(clip->referencedClip()->fileURL(),
				//	countChannel, (int) timeline()->mapLocalToValue(i),
				//	1.0, width, i, y + deltaHeight * countChannel,
				//	h / channels, ex, painter));
			}
	    }
	}
        
    }
    
    void TrackViewAudioBackgroundDecorator::drawChannel(int channel,
	const QByteArray * ba, int x, int y, int height, int maxWidth,
	QPainter & painter) {
		
	for (int a = 0; a < ba->size(); a++) {
	    int val = (*ba)[a] * (height / 2) / 128;

	    if (a + x >= maxWidth || !painter.isActive())
		return;
	    painter.drawLine(a + x, y + height / 2 - val, a + x,
		y + height / 2 + val);
	}
    }

    void TrackViewAudioBackgroundDecorator::
	setSoundSamples(const KURL & url, int channel, int frame,
	double frameLength, const QByteArray & array, int x, int y,
	int height, int maxWidth, QPainter & painter) {

	drawChannel(channel, &array, x, y, height, maxWidth, painter);
    }

};
