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
#include <qstring.h>

#include "docclipbase.h"

/**DocTrackBase is a base class for all real track entries into the database.
It provides core functionality that all tracks must possess.
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
		* This method calls canAddClip() to determine whether or not the clip can be added to this
		* particular track. */
  bool addClip(DocClipBase *clip);
  /** Returns the clip type as a string. This is a bit of a hack to give the
		* KMMTimeLine a way to determine which class it should associate
		*	with each type of clip. */
  virtual QString clipType() = 0;
  /** Returns an iterator to the clip after the last clip(chronologically) which overlaps the start/end value range specified.

This allows you to write standard iterator for loops over a specific range of the track. */
  QPtrListIterator<DocClipBase> endClip(double startValue, double endValue);
  /** Returns an iterator to the first clip (chronologically) which overlaps the start/end value range specified. */
  QPtrListIterator<DocClipBase> firstClip(double startValue, double endValue);
  /** selects all clips in this track. */
  void selectAll();
  /** Clears the selection of all clips in this track. */
  void selectNone();  
  /** Toggles the selected state of the clip at the given value. If it was true, it becomes false, if it was false, it becomes true. */
  bool toggleSelectClipAt(int value);
  /** Make the clip which exists at the given value selected. */
  bool selectClipAt(int value);
private: // Private attributes
public: // Public attributes
  /** Contains a list of all of the clips within this track. */
  QPtrList<DocClipBase> m_clips;
};

#endif
