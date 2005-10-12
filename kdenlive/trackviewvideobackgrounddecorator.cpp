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
#include "trackviewvideobackgrounddecorator.h"

#include <qpainter.h>

#include "docclipref.h"
#include "gentime.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"
#include <qimage.h>

//only for testing please remove if video get drawn
#include <kstandarddirs.h>
namespace Gui{
TrackViewVideoBackgroundDecorator::TrackViewVideoBackgroundDecorator(KTimeLine* timeline,
												KdenliveDoc* doc,
												const QColor &selected,
												const QColor &unselected,
												int shift) :
									DocTrackDecorator(timeline, doc),
									m_selected(selected),
									m_unselected(unselected),
									m_shift(shift)
{
	
}


TrackViewVideoBackgroundDecorator::~TrackViewVideoBackgroundDecorator()
{
}

// virtual
void TrackViewVideoBackgroundDecorator::paintClip(double startX, double endX, QPainter &painter, DocClipRef *clip, QRect &rect, bool selected)
{
	int sx = startX; // (int)timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));
	int ex = endX; //(int)timeline()->mapValueToLocal(clip->trackEnd().frames(document()->framesPerSecond()));

	/*if(sx < rect.x()) {
		sx = rect.x();
	}
	if(ex > rect.x() + rect.width()) {
		ex = rect.x() + rect.width();
}*/
	//ex -= sx;
	int y=rect.y();
	int h=rect.height();
	if (m_shift){
		h-=m_shift;
	}
	QColor col = selected ? m_selected : m_unselected;
 	//qDebug("%f",clip->clipWidth()+clip->clipHeight());
	double aspect=4.0/3.0;
	int width=(h)*aspect;
	int i=sx;
	/** this is only for testingmust be replaced with picture from video **/

	QPixmap img(locate( "appdata", "kdenlive-splash.png" ) );
	/*QImage img(rect.height()*aspect,rect.height(),32);
	img.fill(127);
	img.setOffset(QPoint(rect.height()*aspect/2,rect.height()/2));
	img.setColor(0,qRgb(255,255,255));
	img.setText("","","Picture");*/
					  
	////////
	for (;i+width<ex;i+=width){
		if (i+width<rect.x() || i>rect.x()+rect.width())
			continue;
		//painter.fillRect( i, rect.y(),width,rect.height(), col);
		painter.drawPixmap(i,y,img,0,0,width,h);
		painter.drawRect( i, y,width,h);	
	}
	if (i<ex){
		//painter.fillRect(i, rect.y(),ex-i, rect.height(), col);
		painter.drawPixmap(i,y,img,0,0,ex-i,h);
		painter.drawRect( i, y,ex-i, h);
	}
	
	
}

};
