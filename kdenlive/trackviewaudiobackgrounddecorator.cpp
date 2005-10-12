/***************************************************************************
                          trackviewbackgrounddecorator  -  description
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
#include "trackviewaudiobackgrounddecorator.h"

#include <qpainter.h>

#include "docclipref.h"
#include "gentime.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"
#include <qimage.h>

//only for testing please remove if video get drawn
#include <kstandarddirs.h>
namespace Gui{
TrackViewAudioBackgroundDecorator::TrackViewAudioBackgroundDecorator(KTimeLine* timeline,
												KdenliveDoc* doc,
												const QColor &selected,
												const QColor &unselected,
												int size) :
									DocTrackDecorator(timeline, doc),
									m_selected(selected),
									m_unselected(unselected),
									m_height(size)
{
}


TrackViewAudioBackgroundDecorator::~TrackViewAudioBackgroundDecorator()
{
}

// virtual
void TrackViewAudioBackgroundDecorator::paintClip(double startX, double endX, QPainter &painter, DocClipRef *clip, QRect &rect, bool selected)
{
	int sx = startX; // (int)timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));
	int ex = endX; //(int)timeline()->mapValueToLocal(clip->trackEnd().frames(document()->framesPerSecond()));

	if(sx < rect.x()) {
		sx = rect.x();
	}
	if(ex > rect.x() + rect.width()) {
		ex = rect.x() + rect.width();
	}
	//ex -= sx;
	int y=rect.y();
	int h=rect.height();
	
	if (m_height>0){
		y+=(h-m_height);
		h=m_height;
	}
	QColor col = selected ? m_selected : m_unselected;
	double aspect=4.0/3.0;
	int width=(int)(h)*aspect;
	int i=sx;
	/** this is only for testing must be replaced with picture from video **/

	int soundbufferLen=1000;
	short soundbuffer[soundbufferLen];
	for(int i=0;i<soundbufferLen;i++){
		soundbuffer[i]=sin((2.0*3.14259265*(double)i)/(double)soundbufferLen)*32000.0;
	}
	////////////////////
	painter.drawRect( sx, y, ex-sx, h);
	painter.fillRect( sx, y, ex-sx, h,col);
	for (;i<ex;i+=width){
		if (i+width<rect.x() || i>rect.x()+rect.width())
			continue;
		int xm=i,a=0;
		int soundheight=h;
		
		for (;a<soundbufferLen && xm < ex;a+=soundbufferLen/width,xm+=1){
			int val=abs(soundbuffer[a])*(soundheight/2)/32000;
			painter.drawLine(
				xm,y+soundheight/2-val,
				xm,y+soundheight/2+val
			);	
		}

	}

	
	
}

};
