/***************************************************************************
                          kmmtrackvideo.h  -  description
                             -------------------
    begin                : Fri Apr 12 2002
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

#ifndef KMMTRACKVIDEO_H
#define KMMTRACKVIDEO_H

#include <qwidget.h>

#include <doctrackvideo.h>
#include <kmmtrackbase.h>

/**KMMTrackVideo provides the timeline view for video clips
  *@author Jason Wood
  */

class KMMTrackVideo : public KMMTrackBase  {
   Q_OBJECT
public: 
	KMMTrackVideo(DocTrackVideo *docTrack, QWidget *parent=0, const char *name=0);
	~KMMTrackVideo();
};

#endif
