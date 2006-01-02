/***************************************************************************
                          trackviewvideobackgrounddecorator  -  description
                             -------------------
    begin                : May 2005
    copyright            : (C) 2005 Marco Gittler
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
#include "trackviewvideobackgrounddecorator.h"

#include <qpainter.h>

#include "docclipref.h"
#include "gentime.h"
#include "kdenlivedoc.h"
#include "kdebug.h"
#include "ktimeline.h"
#include <qimage.h>
#include <iostream>

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
	//QDomDocument myxml=clip->generateSceneList();
	//qDebug("%s\n",myxml.toString().ascii());
	
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
	if (m_shift){
		h-=m_shift;
	}
	QColor col = selected ? m_selected : m_unselected;
 	//qDebug("%f",clip->clipWidth()+clip->clipHeight());
	double aspect=4.0/3.0;
	//width of a frame shown in timeline
	int width=(int)timeline()->mapValueToLocal(1)-(int)timeline()->mapValueToLocal(0);
	int width1=(h)*aspect;
	if (width1>width)
		width=width1;
	int i=sx;
	int frame=0;

	/* Use the clip's default thumbnail & scale it to track size to decorate until we have some better stuff */ 
	QPixmap newimg = clip->referencedClip()->thumbnail();
	QImage im;
	im = newimg;
	newimg = im.scale(width,h);

	for (;i<ex;i+=width){
		//document()->renderer()->getImage(document()->URL(),frame+=10,&newimg);
		int drawWidth=width;
		if (i+width>ex)
			drawWidth=ex-i;
		painter.drawPixmap(i, y ,newimg,0,0,drawWidth,h);
		painter.drawRect( i, y,drawWidth,h);	
	}
	
}

};
