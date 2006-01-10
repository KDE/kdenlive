/***************************************************************************
                          trackviewdoublekeyframedecorator  -  description
                             -------------------
    begin                : Fri Jan 9 2004
    copyright            : (C) 2004 by Jason Wood
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
#include  <qpainter.h>
#include  <kdebug.h>

#include "trackviewdoublekeyframedecorator.h"
#include "ktimeline.h"
#include "effectparameter.h"
#include "effectparamdesc.h"
#include "effectkeyframe.h"
#include "effectdoublekeyframe.h"
#include "docclipref.h"
#include "gentime.h"
#include "kdenlivedoc.h"

namespace Gui
{

TrackViewDoubleKeyFrameDecorator::TrackViewDoubleKeyFrameDecorator(KTimeLine *timeline, KdenliveDoc *doc, const QString &effectName, int effectIndex, const QString &paramName)
 							: DocTrackDecorator(timeline, doc),
							m_effectName(effectName),
							m_effectIndex(effectIndex),
							m_paramName(paramName)
{
}


TrackViewDoubleKeyFrameDecorator::~TrackViewDoubleKeyFrameDecorator()
{
}

// virtual
void TrackViewDoubleKeyFrameDecorator::paintClip(double startX, double endX, QPainter &painter, DocClipRef *clip, QRect &rect, bool selected)
{
	if (!clip->hasEffect()) return;
	int sx = startX; // (int)timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));
	int ex = endX; //(int)timeline()->mapValueToLocal(clip->trackEnd().frames(document()->framesPerSecond()));

	/*if(sx < rect.x()) {
		sx = rect.x();
	}
	if(ex > rect.x() + rect.width()) {
		ex = rect.x() + rect.width();
	}*/
	ex -= sx;

	int ey = rect.height();
	int sy = rect.y() + ey;



	// draw outline box
//	painter.fillRect( sx, rect.y(), ex, rect.height(), col);
	
	m_effectIndex = 0;
	m_effect = clip->selectedEffect();

	if (! m_effect) {
		kdDebug()<<"////// ERROR, EFFECT NOT FOUND"<<endl;
		return;
		}

	if (m_effect->parameter(m_effectIndex) && m_effect->effectDescription().parameter(m_effectIndex)->type()=="double")
	{
	kdDebug()<<"+++++ EFFECT KEYFR. FOUND"<<m_effect->parameter(m_effectIndex)->numKeyFrames()<<endl;

	painter.setPen(Qt::red);
	uint count = m_effect->parameter(m_effectIndex)->numKeyFrames();

	if (count > 1 ) {
	uint start = m_effect->parameter(m_effectIndex)->keyframe(0)->time()*ex;
	painter.fillRect(sx + start +1, rect.y()+1, m_effect->parameter(m_effectIndex)->keyframe(count-1)->time()*ex-2-start, rect.height()-2, QBrush(Qt::white));	
		for (uint i = 0; i<count -1; i++)
		{
		uint dx1 = sx + m_effect->parameter(m_effectIndex)->keyframe(i)->time()*ex;
		uint dy1 = sy - ey*m_effect->parameter(m_effectIndex)->keyframe(i)->toDoubleKeyFrame()->value()/100;
		uint dx2 = sx + m_effect->parameter(m_effectIndex)->keyframe(i+1)->time()*ex;
		uint dy2 = sy - ey*m_effect->parameter(m_effectIndex)->keyframe(i+1)->toDoubleKeyFrame()->value()/100;
		//kdDebug()<<"++++++ DRAWING KEYFRAME : "<<dx1<<", "<<dy1<<", "<<dx2<<", "<<dy2<<endl;

		painter.fillRect(dx1-3, dy1-3, 6, 6, QBrush(Qt::red));
		painter.fillRect(dx2-3, dy2-3, 6, 6, QBrush(Qt::red));
		painter.drawLine (dx1, dy1, dx2, dy2);
		}
	}
	painter.setPen(Qt::black);
	}

}

} // namespace Gui