/***************************************************************************
                          kmmtrackpanel.h  -  description
                             -------------------
    begin                : Tue Aug 6 2002
    copyright            : (C) 2002 by Jason Wood
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

#ifndef KMMTRACKPANEL_H
#define KMMTRACKPANEL_H

#include <qpixmap.h>
#include <qpainter.h>
#include <qframe.h>

#include "doctrackbase.h"

class KMMTimeLine;

/**Base class for all Track panels and their associated views.
  *@author Jason Wood
  */

class KMMTrackPanel : public QFrame  {
public: 
	KMMTrackPanel(KMMTimeLine &timeline, DocTrackBase & docTrack, QWidget *parent, const char *name);
	~KMMTrackPanel();
  /** Read property of KMMTimeLine * m_timeline. */
  KMMTimeLine & timeLine();
  /** returns the document track which is displayed by this track */
  DocTrackBase & docTrack();
  /** This function will paint a clip on screen. This funtion must be provided by a derived class. */
	virtual void paintClip(QPainter &painter, DocClipBase *clip, QRect &rect, bool selected) = 0;
	/** Paints the backbuffer into the relevant place using the painter supplied. The track should be drawn into
	the area provided in area */
	void drawToBackBuffer(QPainter &painter, QRect &rect);	
public slots:
	void resize(QRect size);
protected: // Protected attributes
  /** The track document class that should be queried to build up this track view. */
  DocTrackBase &m_docTrack;
private:
  /** The KMMTrackPanel needs access to various methods from it's parents Timeline. The parent timeline
  	 is stored in this variable. */
  KMMTimeLine & m_timeline;
};

#endif
