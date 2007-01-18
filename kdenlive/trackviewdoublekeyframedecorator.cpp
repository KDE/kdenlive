/***************************************************************************
                          trackviewdoublekeyframedecorator  -  description
                             -------------------
    begin                : Fri Jan 9 2004
    copyright            : (C) 2004 Jason Wood, jasonwood@blueyonder.co.uk
			 : (C) 2006 Jean-Baptiste Mardelle, jb@ader.ch

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

namespace Gui {

    TrackViewDoubleKeyFrameDecorator::
	TrackViewDoubleKeyFrameDecorator(KTimeLine * timeline, KdenliveDoc * doc)
    :DocTrackDecorator(timeline, doc) {
    } 

    TrackViewDoubleKeyFrameDecorator::~TrackViewDoubleKeyFrameDecorator() {
    }

// virtual
    void TrackViewDoubleKeyFrameDecorator::paintClip(double startX,
	double endX, QPainter & painter, DocClipRef * clip, QRect & rect,
	bool selected) {
	if (!clip->hasEffect())
	    return;
	m_effect = clip->selectedEffect();
	if (!m_effect || !m_effect->isEnabled()) {
	    return;
	}
	int sx = (int)startX;	
	int ex = (int)endX;
	//ex -= sx;
	int clipWidth = ex - sx;

	if (sx < rect.x()) {
            sx = rect.x();
	}
	if (ex > rect.x() + rect.width()) {
	    ex = rect.x() + rect.width();
	}

	int ey = rect.height();
	int sy = rect.y() + ey;

	int effectIndex = 0;
	
	if (m_effect->parameter(effectIndex)) {
	    painter.setClipRect(sx - 2, rect.y(), ex - sx + 4, rect.height());
	    if (m_effect->effectDescription().parameter(effectIndex)->
		type() == "double") {

		uint count = m_effect->parameter(effectIndex)->numKeyFrames();
		QBrush brush(Qt::red);

		if (count > 1) {
		    uint start =(uint)(m_effect->parameter(effectIndex)->keyframe(0)->time() * clipWidth);

		    painter.setPen(Qt::red);
		    int selectedKeyFrame = m_effect->parameter(effectIndex)->selectedKeyFrame();
		    double factor = m_effect->effectDescription().parameter(effectIndex)->max();

		    for (int i = 0; i < (int)count - 1; i++) {
			int dx1 =(int)( startX +  m_effect->parameter(effectIndex)->keyframe(i)->time() * clipWidth);
			int dy1 =(int)( sy - ey * m_effect->parameter(effectIndex)->keyframe(i)->toDoubleKeyFrame()->value() / factor);
			int dx2 =(int)( startX + m_effect->parameter(effectIndex)->keyframe(i + 1)->time() * clipWidth);
			int dy2 =(int)( sy - ey * m_effect->parameter(effectIndex)->keyframe(i + 1)->toDoubleKeyFrame()->value() / factor);

			// #HACK: if x coordinates go beyond max int values, the drawLine method
			// gives strange results, so limit it for the moment...
			if (dx1 < -32700) dx1 = -32700;
			if (dx2 > 32700) dx2 = 32700;
			if (dx1 <= ex) {
			    if (dx2 > ex && dx1 > ex ) break; 
			    if (i == selectedKeyFrame)
			    	brush = QBrush(Qt::blue);
			    else
			    	brush = QBrush(Qt::red);
			    painter.fillRect(dx1 - 3, dy1 - 3, 6, 6, brush);
			    if (i + 1 == selectedKeyFrame)
			    	brush = QBrush(Qt::blue);
			    else
			    	brush = QBrush(Qt::red);
			    painter.fillRect(dx2 - 3, dy2 - 3, 6, 6, brush);
			    painter.drawLine(dx1, dy1, dx2, dy2);
			}
		    }
		}
		painter.setPen(Qt::black);
	    } else if (m_effect->effectDescription().
		parameter(effectIndex)->type() == "complex") {

		uint count =
		    m_effect->parameter(effectIndex)->numKeyFrames();
		QBrush brush(Qt::red);

		if (count > 1) {
		    painter.setPen(Qt::red);
		    int selectedKeyFrame = m_effect->parameter(effectIndex)->selectedKeyFrame();
		    for (int i = 0; i < (int)count - 1; i++) {
			int dx1 =(int)( startX + m_effect->parameter(effectIndex)->keyframe(i)->time() * clipWidth);
			int dy1 = sy - ey / 2;
			int dx2 =(int)( startX + m_effect->parameter(effectIndex)->keyframe(i + 1)->time() * clipWidth);
			int dy2 = sy - ey / 2;

			if (i == selectedKeyFrame)
			    brush = QBrush(Qt::blue);
			else
			    brush = QBrush(Qt::red);

			// #HACK: if x coordinates go beyond max int values, the drawLine method
			// gives strange results, so limit it for the moment...
			if (dx1 < -32700) dx1 = -32700;
			if (dx2 > 32700) dx2 = 32700;
			if (dx1 <= ex) {
			    if (dx2 > ex && dx1 > ex ) break; 
			    painter.fillRect(dx1 - 3, dy1 - 3, 6, 6, brush);
			    if (i + 1 == selectedKeyFrame)
			        brush = QBrush(Qt::blue);
			    else
			        brush = QBrush(Qt::red);
			    painter.fillRect(dx2 - 3, dy2 - 3, 6, 6, brush);
			    painter.drawLine(dx1, dy1, dx2, dy2);
			}
		    }
		}
		painter.setPen(Qt::black);
	    }
	}
    painter.setClipping(false);
    }
}				// namespace Gui
