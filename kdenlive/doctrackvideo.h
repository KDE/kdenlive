/***************************************************************************
                          doctrackvideo.h  -  description
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

#ifndef DOCTRACKVIDEO_H
#define DOCTRACKVIDEO_H

#include "doctrackbase.h"

/**DocTrackVideo holds all data about a video track
  *@author Jason Wood
  */

class DocTrackVideo : public DocTrackBase  {
public: 
	DocTrackVideo();
	~DocTrackVideo();
  /** Returns true if the specified clip can be added to this track, false otherwise. */
  bool canAddClip(DocClipBase * clip);
  /** Returns the clip type as a string. This is a bit of a hack to give the
		* KMMTimeLine a way to determine which class it should associate
		*	with each type of clip. */
  QString clipType();
};

#endif
