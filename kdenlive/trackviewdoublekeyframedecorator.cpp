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
	int sx = (int)startX;	// (int)timeline()->mapValueToLocal(clip->trackStart().frames(document()->framesPerSecond()));
	int ex = (int)endX;		//(int)timeline()->mapValueToLocal(clip->trackEnd().frames(document()->framesPerSecond()));

	/*if(sx < rect.x()) {
	   sx = rect.x();
	   }
	   if(ex > rect.x() + rect.width()) {
	   ex = rect.x() + rect.width();
	   } */
	ex -= sx;

	int ey = rect.height();
	int sy = rect.y() + ey;



	// draw outline box
//      painter.fillRect( sx, rect.y(), ex, rect.height(), col);

	int effectIndex = 0;
	m_effect = clip->selectedEffect();

	if (!m_effect) {
	    kdDebug() << "////// ERROR, EFFECT NOT FOUND" << endl;
	    return;
	}

	if (m_effect->parameter(effectIndex)) {
	    if (m_effect->effectDescription().parameter(effectIndex)->
		type() == "double") {

		uint count =
		    m_effect->parameter(effectIndex)->numKeyFrames();
		QBrush brush(Qt::red);

		if (count > 1) {
			uint start =(uint)(
			m_effect->parameter(effectIndex)->keyframe(0)->
			time() * ex);
		    /*painter.fillRect(sx + start + 1, rect.y() + 1,
			(int)(m_effect->parameter(effectIndex)->
			keyframe(count - 1)->time() * ex - 2 - start),
			rect.height() - 2, QBrush(Qt::white));*/
		    painter.drawRect(sx + start, rect.y(),
			(int)(m_effect->parameter(effectIndex)->
			keyframe(count - 1)->time() * ex - start),
			rect.height());

		    painter.setPen(Qt::red);
		    int selectedKeyFrame =
			m_effect->parameter(effectIndex)->
			selectedKeyFrame();
			 for (int i = 0; i < (int)count - 1; i++) {
				 uint dx1 =(uint)(
			    sx +
			    m_effect->parameter(effectIndex)->
				keyframe(i)->time() * ex);
				 uint dy1 =(uint)(
			    sy -
			    ey *
			    m_effect->parameter(effectIndex)->
				keyframe(i)->toDoubleKeyFrame()->value() / 100);
				 uint dx2 =(uint)(
			    sx +
			    m_effect->parameter(effectIndex)->
						 keyframe(i + 1)->time() * ex);
				 uint dy2 =(uint)(
			    sy -
			    ey *
			    m_effect->parameter(effectIndex)->
			    keyframe(i +
				1)->toDoubleKeyFrame()->value() / 100);
			//kdDebug()<<"++++++ DRAWING KEYFRAME : "<<dx1<<", "<<dy1<<", "<<dx2<<", "<<dy2<<endl;
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
		painter.setPen(Qt::black);
	    } else if (m_effect->effectDescription().
		parameter(effectIndex)->type() == "complex") {

		uint count =
		    m_effect->parameter(effectIndex)->numKeyFrames();
		QBrush brush(Qt::red);

		if (count > 1) {
			uint start =(uint)(
			m_effect->parameter(effectIndex)->keyframe(0)->
			time() * ex);
		    painter.fillRect(sx + start + 1, rect.y() + 1,
			(int)(m_effect->parameter(effectIndex)->
			keyframe(count - 1)->time() * ex - 2 - start),
			rect.height() - 2, QBrush(Qt::white));
		    painter.drawRect(sx + start, rect.y(),
			(int)(m_effect->parameter(effectIndex)->
			keyframe(count - 1)->time() * ex - start),
			rect.height());

		    painter.setPen(Qt::red);
		    int selectedKeyFrame =
			m_effect->parameter(effectIndex)->
			selectedKeyFrame();
			 for (int i = 0; i < (int)count - 1; i++) {
				 uint dx1 =(uint)(
			    sx +
			    m_effect->parameter(effectIndex)->
				keyframe(i)->time() * ex);
			uint dy1 = sy - ey / 2;
			uint dx2 =(uint)(
			    sx +
			    m_effect->parameter(effectIndex)->
				keyframe(i + 1)->time() * ex);
			uint dy2 = sy - ey / 2;

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
		painter.setPen(Qt::black);
	    }
	}

    }

}				// namespace Gui
