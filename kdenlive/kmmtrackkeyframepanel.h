/***************************************************************************
                          kmmtrackkeyframepanel.h  -  description
                             -------------------
    begin                : Sun Dec 1 2002
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

#ifndef KMMTRACKKEYFRAMEPANEL_H
#define KMMTRACKKEYFRAMEPANEL_H

#include <qwidget.h>
#include <kmmtrackpanel.h>

#include "doctrackbase.h"

/**This panel widget handles the manipulation of keyframes.
  *@author Jason Wood
  */

class KMMTrackKeyFramePanel : public KMMTrackPanel  {
   Q_OBJECT
public: 
	KMMTrackKeyFramePanel(KMMTimeLine &timeline, DocTrackBase &doc, QWidget *parent=0, const char *name=0);
	~KMMTrackKeyFramePanel();
  /** This function will paint a clip on screen, using the specified painter and the given coordinates as to where the clip should be painted. */
  void paintClip(QPainter & painter, DocClipBase * clip, QRect & rect, bool selected);
};

#endif
