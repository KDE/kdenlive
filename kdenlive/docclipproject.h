/***************************************************************************
                          docclipproject.h  -  description
                             -------------------
    begin                : Thu Jun 20 2002
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

#ifndef DOCCLIPPROJECT_H
#define DOCCLIPPROJECT_H

#include "docclipbase.h"

/**This "clip" consists of a number of tracks, clips, overlays, transitions and effects. It is basically capable of making multiple clips accessible as if they were a single clip. The "clipType()" of this clip depends entirely upon it's contents.
  *@author Jason Wood
  */

class DocClipProject : public DocClipBase  {
public: 
	DocClipProject();
	~DocClipProject();

	long duration();
  
	/** No descriptions */
	KURL fileURL();

	QDomDocument toXML();
};

#endif
