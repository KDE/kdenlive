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
#include <qimage.h>

#include <klocale.h>
#include <kiconloader.h>
#include <kstandarddirs.h>

#include "docclipref.h"
#include "gentime.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"

namespace Gui {
    TrackViewAudioBackgroundDecorator::
	TrackViewAudioBackgroundDecorator(KTimeLine * timeline,
	KdenliveDoc * doc, const QColor & unselected, bool limitedThumbs, bool shift):DocTrackDecorator(timeline,
	doc),m_shift(shift), m_unselected(unselected), m_limitedAudioThumbs(limitedThumbs)
{
	m_selected = m_unselected.light(140);
	m_overlayPixmap = QPixmap (locate("appdata", "graphics/lighten.png"));
} 

TrackViewAudioBackgroundDecorator::~TrackViewAudioBackgroundDecorator() 
{
}

// virtual
void TrackViewAudioBackgroundDecorator::paintClip(double startX,
	double endX, QPainter & painter, DocClipRef * clip, QRect & rect,
	bool selected) {
	if (!clip->referencedClip() || clip->audioChannels() == 0 || clip->speed() != 1.0) return;
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

	if (m_shift) {
	    h = h/3;
	    y += 2*h;
	}

	QColor col = selected ? m_selected : m_unselected;
	//double aspect = 4.0 / 3.0;
	int width = ex - sx;
	
	double FramesInOnePixel = (timeline()->mapValueToLocal(1) - timeline()->mapValueToLocal(0));

	if (m_limitedAudioThumbs && FramesInOnePixel < 0.1 && ((endX - startX) / FramesInOnePixel  > 4000)) {
	    if (!m_shift) painter.fillRect(sx, y, ex - sx, h, col);
	    return;
	}

	FramesInOnePixel = width / FramesInOnePixel;

	double i = sx;

	int channels =clip->audioChannels();
	if (channels==0)
	    channels=1;

	painter.setClipRect(sx, y, ex - sx, h);
	if (!m_shift) {
	    // Audio track
	    painter.fillRect(sx, y, ex - sx, h, col);
	}
	else {
	    // video track
	    int newx1 = sx;
	    int newx2 = ex;
	    int clipw = clip->thumbnailWidth();
	    if (clipw > endX - startX) clipw = endX - startX;
	    if (newx1 < startX + clipw) {
		// draw semi transparent pixmap over thumbnails to lighten them
		painter.drawPixmap(startX, y, m_overlayPixmap, 0, 0, clipw, h);
		newx1 = startX + clipw;
	    }

	    if (newx2 > endX - clipw) {
		// draw semi transparent pixmap over thumbnails to lighten them
		painter.drawPixmap(endX - clipw, y, m_overlayPixmap, 0, 0, clipw, h);
	        newx2 = endX - clipw;
	    }
            // draw rect between thumbnails
	    if (newx2 > newx1) painter.fillRect(newx1, y, newx2 - newx1, h, col);
	}
	painter.setPen(col.dark(160));
	double timeDiff = clip->cropStartTime().frames(document()->framesPerSecond()) - clip->trackStart().frames(document()->framesPerSecond());
	

	if (clip->referencedClip()->audioThumbCreated == false) {
		if (clip->clipType() == DocClipBase::AV || clip->clipType() == DocClipBase::AUDIO) {
			QPixmap pixmap(KGlobal::iconLoader()->loadIcon("run", KIcon::Toolbar));
			painter.drawPixmap((int)startX, rect.y(), pixmap);
		}
	}
	else for (; i < ex; i += width) {
		if (i + width < rect.x() || i > rect.x() + rect.width())
			continue;
		int deltaHeight = h / channels;
		double RealFrame = timeline()->mapLocalToValue(i);
		for (int countChannel = 0; countChannel < channels;countChannel++) {
		{	
			QByteArray a=clip->getAudioThumbs(countChannel,
					RealFrame + timeDiff,
					FramesInOnePixel, width);
			drawChannel(&a,(int)i,y + deltaHeight * countChannel,h / channels, ex,painter);
			}
	    }
	}
	painter.setPen(Qt::black);
	painter.setClipping(false);
}

void TrackViewAudioBackgroundDecorator::drawChannel(const QByteArray * ba,
	int x, int y, int height, int maxWidth,
	QPainter & painter) {
		
	for (int a = 0; a < (int)ba->size(); a++) {
		int val = (*ba)[a] * (height / 2) / 128;
		if (a + x >= maxWidth || !painter.isActive())
			return;	
		painter.drawLine(a + x, y + height / 2 - val, a + x,
			y + height / 2 + val);
	}
}


};
