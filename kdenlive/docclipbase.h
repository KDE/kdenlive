/***************************************************************************
                          docclipbase.h  -  description
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

#ifndef DOCCLIPBASE_H
#define DOCCLIPBASE_H

#include <avfile.h>

/**DocClipBase is a base class for the various types of clip
  *@author Jason Wood
  */

class DocClipBase {
public: 
	DocClipBase(AVFile * avFile);
	virtual ~DocClipBase() = 0;
  /** Returns where this clip starts on the track */
  long trackStart();
  /** Returns the AVFile object which defines the object which is used by this clip. */
  AVFile * avFile();
private: // Private attributes
  /** Where this clip starts on the track that it resides on. */
  long m_trackStart;
  /** The avFile that this clip is based upon.  */
  AVFile * m_avFile;
};

#endif
