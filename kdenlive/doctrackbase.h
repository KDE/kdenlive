/***************************************************************************
                          doctrackbase.h  -  description
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

#ifndef DOCTRACKBASE_H
#define DOCTRACKBASE_H

#include <qlist.h>
#include "docclipbase.h"

/**DocTrackBase is a base class for all real track entries into the database.
It provides core functionality that all tracks must posess.
  *@author Jason Wood
  */

class DocTrackBase {
public: 
	DocTrackBase();
	virtual ~DocTrackBase();
  /** Returns true if the specified clip can be added to this track, false otherwise.
		*
		* This method needs to be implemented by inheriting classes to define
		* which types of clip they support. */
  virtual bool canAddClip(DocClipBase *clip) = 0;
  /** Adds a clip to the track. Returns true if successful, false if it fails for some reason.
		*
		* This method calls canAddClip() to determine whether or not the clip can be added to this
		* particular track. */
  bool addClip(DocClipBase *clip);
private: // Private attributes
  /** Contains a list of all of the clips within this track. */
  QList<DocClipBase> m_clips;
};

#endif
