/***************************************************************************
                          clipgroup.h  -  description
                             -------------------
    begin                : Thu Aug 22 2002
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

#ifndef CLIPGROUP_H
#define CLIPGROUP_H

#include "docclipbaselist.h"

class KdenliveDoc;

/**A clip group is a grouping of clips. It can be used as a simple way to apply transformations to multiple
  *clips at once, which may extend over multiple tracks. It handles any special cases, such as moving clips
  *between tracks and associated problems.
  *
  *A clip group does not own clips placed into it, and does not delete them either. It's use is merely for
  *grouping, not for maintaining hierarchical structure. Clip groups cannot contain other clip groups.
  *   
  *@author Jason Wood
  */

class ClipGroup {
public: 
	ClipGroup();
	~ClipGroup();
  /** Adds a clip to the clip group. If the clip is already in the group, it will not be added a
   second time. */
  void addClip(DocClipBase *clip);
  /** Removes the selected clip from the clip group. If the clip passed is not in the clip group, nothing
   (other than a warning) will happen. */
  void removeClip(DocClipBase *clip);
  /** Returns true if the specified clip is in the group. */
  bool clipExists(DocClipBase *clip);
  /** Removes the clip if it is in the list, adds it if it isn't. Returns true if the clip is now part
   of the list, false if it is not. */
  bool toggleClip(DocClipBase *clip);
  /** Moves all clips by the offset specified. Returns true if successful, fasle on failure. */
  bool moveByOffset(int offset);
  /** Removes all clips from the clip group.
 */
  void removeAllClips();
  /** Find space on the timeline where the clips in the clipGroup will fit. Returns true if successful,
   initialising newValue to the given offset. If no place can be found, returns false. */
  bool findClosestMatchingSpace(int value, int &newValue);
  /** Returns a clip list of all clips contained within the group. */
  QPtrList<DocClipBase> clipList();
  /** returns true is this clipGroup is currently empty, false otherwise. */
  bool isEmpty();
  /** No descriptions */
  void setMasterClip(DocClipBase *master);
  /** Moves all clips from their current location to a new location. The location points specified
  are those where the current Master Clip should end up. All other clips move relative to this clip.
   Only the master clip necessarily needs to reside on the specified track. Clip priority will never
   change - Display order will be preserved.*/
  void moveTo(KdenliveDoc &doc, int track, int value);
  /** Returns an XML representation of this clip group */
  QDomDocument toXML();
  /** Returns a url list of all of the clips in this group.  */
  KURL::List createURLList();
private: // Private methods
	DocClipBaseList m_clipList;
  /** Pointer to the current master clip. */	
  DocClipBase * m_master;
};

#endif
