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

#include <qobject.h>
#include <qlist.h>
#include <qstring.h>

#include "docclipbaselist.h"

/**DocTrackBase is a base class for all real track entries into the database.
It provides core functionality that all tracks must possess.
  *@author Jason Wood
  */

class DocTrackBase : public QObject
{
	Q_OBJECT
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
  /** Returns the clip which resides at the current value, or 0 if non exists */
  DocClipBase *getClipAt(int value);
  /** Adds all of the clips in the pointerlist into this track. */
  void addClips(DocClipBaseList list);
  /** returns true if all of the clips within the cliplist can be added, returns false otherwise. */
  bool canAddClips(DocClipBaseList clipList);;
  /** Returns true if the clip given exists in this track, otherwise returns
false. */
  bool clipExists(DocClipBase *clip);
  /** Removes the given clip from this track. If it doesn't exist, then
			a warning is issued vi kdWarning, but otherwise no bad things
			will happen. The clip is removed from the track but NOT deleted.*/
  void removeClip(DocClipBase *clip);
  /** The clip specified has moved. This method makes sure that the clips
are still in the correct order, rearranging them if they are not. */
  void clipMoved(DocClipBase *clip);
public: // Public attributes
  /** Contains a list of all of the clips within this track. */
  DocClipBaseList m_clips;
signals:
	/** Emitted whenever the track changes.*/
	void trackChanged();
};

#endif
