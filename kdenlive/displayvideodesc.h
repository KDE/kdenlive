/***************************************************************************
                          displayvideodesc.h  -  description
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

#ifndef DISPLAYVIDEODESC_H
#define DISPLAYVIDEODESC_H


/**This class holds the description of a video format that the renderer can use to display images on the screen. Common examples are xv, rgb, and fb.
  *@author Jason Wood
  */

class DisplayVideoDesc {
public: 
	DisplayVideoDesc();
	~DisplayVideoDesc();
};

#endif
