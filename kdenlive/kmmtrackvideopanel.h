/***************************************************************************
                          kmmtrackvideopanel.h  -  description
                             -------------------
    begin                : Tue Apr 9 2002
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

#ifndef KMMTRACKVIDEOPANEL_H
#define KMMTRACKVIDEOPANEL_H

#include <qwidget.h>
#include <qlabel.h>
#include <qhbox.h>
#include <qpixmap.h>

#include "doctrackvideo.h"
#include "kmmtrackpanel.h"

/**KMMTrackVideoPanel contains useful controls for manipulating the video tracks
which reside in the main video widget
  *@author Jason Wood
  */

class KMMTrackVideoPanel : public KMMTrackPanel  {
   Q_OBJECT
public:
	KMMTrackVideoPanel(KMMTimeLine *timeline, DocTrackVideo *docTrack, QWidget *parent=0, const char *name=0);
	~KMMTrackVideoPanel();
  /** Paint the specified clip on screen within the specified rectangle, using the specified painter. */
  void paintClip(QPainter &painter, DocClipBase *clip, QRect &rect, bool selected);
private:
  QHBox m_horzLayout;
	QLabel m_trackLabel;
  /** True if we are inside a dragging operation, false otherwise. */
  bool m_dragging;

  /** Test image to display on track */
  QPixmap m_pixmap;
};

#endif
