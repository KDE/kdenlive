/***************************************************************************
                          displayaudiodesc.h  -  description
                             -------------------
    begin                : Sun Feb 9 2003
    copyright            : (C) 2003 by Jason Wood
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

#ifndef DISPLAYAUDIODESC_H
#define DISPLAYAUDIODESC_H


/**Contains a description of a possible audio output that the renderer can use when playing video files. Potential examples include oss, mas, arts, alsa.
  *@author Jason Wood
  */

class DisplayAudioDesc {
public: 
	DisplayAudioDesc();
	~DisplayAudioDesc();
};

#endif
