/***************************************************************************
                          kmmtracksound.h  -  description
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

#ifndef KMMTRACKSOUND_H
#define KMMTRACKSOUND_H

#include <qwidget.h>

#include <doctracksound.h>
#include <kmmtrackbase.h>

/**KMMTrackSound provides the timeline view which contains sound clips.
  *@author Jason Wood
  */

class KMMTrackSound : public KMMTrackBase  {
   Q_OBJECT
public: 
	KMMTrackSound(DocTrackSound *docTrack, QWidget *parent=0, const char *name=0);
	~KMMTrackSound();
};

#endif
