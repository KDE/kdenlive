/***************************************************************************
                          doctrackclipiterator.h  -  description
                             -------------------
    begin                : Sat Nov 30 2002
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

#ifndef DOCTRACKCLIPITERATOR_H
#define DOCTRACKCLIPITERATOR_H


#include "doctrackbase.h"

/**Iterates over the clip list of a track, maintaining the order of those clips. This class is necessary because the track puts selected and non-selected clips into seperate lists. This class allows you treat them as a single list again.
  *@author Jason Wood
  */

class DocTrackClipIterator {
  public:
    DocTrackClipIterator(const DocTrackBase & track);
    ~DocTrackClipIterator();
	/** Returns the current clip in the list, or returns 0 otherwise. */
    DocClipRef *current();
	/** Increments the iterator. Works identically to all other iterators. */
    DocClipRef *operator++();
  private:			// Private attributes
	/** An iterator to the selected clip list */
     QPtrListIterator < DocClipRef > *m_selectedItt;
	/** An iterator to the unselected clip list */
     QPtrListIterator < DocClipRef > *m_unselectedItt;
	/** True if the current clip resides in the selected iterator, or false
	if it lies in the unselected iterator. */
    bool m_curSelected;
};

#endif
