/***************************************************************************
                          docclipvideo.h  -  description
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

#ifndef DOCCLIPVIDEO_H
#define DOCCLIPVIDEO_H

#include <docclipbase.h>

/**DocClipVideo contains all the relevant information about a video clip.
  *@author Jason Wood
  */

class DocClipVideo : public DocClipBase  {
public: 
	DocClipVideo(AVFile * avFile);
	~DocClipVideo();
};

#endif
