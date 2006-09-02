/***************************************************************************
                          trackviewaudiobackgrounddecorator  -  description
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
#ifndef TRACKVIEWAUDIOBACKGROUNDDECORATOR_H
#define TRACKVIEWAUDIOBACKGROUNDDECORATOR_H

#include <doctrackdecorator.h>
#include <qpainter.h>
#include <qcolor.h>
#include <qpixmap.h>

#include <ktimeline.h>

class KURL;

/**
Draws the base preview image for a clip; in correct acpect ration and stretch togiven height .

@author Marco Gittler
*/
namespace Gui {

    class TrackViewAudioBackgroundDecorator:public DocTrackDecorator {
      Q_OBJECT public:
	TrackViewAudioBackgroundDecorator(KTimeLine * timeline,
	    KdenliveDoc * doc,
	    const QColor & unselected, bool shift = false);

	 virtual ~ TrackViewAudioBackgroundDecorator();

	virtual void paintClip(double startX, double endx,
	    QPainter & painter, DocClipRef * clip, QRect & rect,
	    bool selected);
	void drawChannel(const QByteArray *, int x, int y,
	    int height, int maxWidth, QPainter & painter);
	/*public slots:void setSoundSamples(const KURL & url, int channel,
	    int frame, double frameLength, const QByteArray & array, int,
	int, int, int, QPainter &);*/
      private:
	bool m_shift;
	QColor m_selected;
	QColor m_unselected;
	QPainter *m_painter;
	QPixmap m_overlayPixmap;
	 //signals:
	    //void getSoundSamples(const KURL & url, int countChannel,
	    //int frame, double frameLength, int arrayWidth, int, int, int,
	    //int, QPainter &);

    };

};
#endif
