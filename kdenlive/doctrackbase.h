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
#include <qvaluelist.h>

#include "docclipbaselist.h"

class KdenliveDoc;
class QString;

/**DocTrackBase is a base class for all real track entries into the database.
It provides core functionality that all tracks must possess.
  *@author Jason Wood
  */

class DocTrackBase : public QObject
{
	Q_OBJECT
public: 
	DocTrackBase(KdenliveDoc *doc);
	virtual ~DocTrackBase();
	/** Returns true if the specified clip can be added to this track, false otherwise.
	*
	* This method needs to be implemented by inheriting classes to define
	* which types of clip they support. */
	virtual bool canAddClip(DocClipBase *clip) = 0;
	/** Adds a clip to the track. Returns true if successful, false if it fails for some reason.
	* This method calls canAddClip() to determine whether or not the clip can be added to this
	* particular track. */
	bool addClip(DocClipBase *clip, bool selected);
	/** Returns the clip type as a string. This is a bit of a hack to give the
	* KMMTimeLine a way to determine which class it should associate
	*	with each type of clip. */
	virtual const QString &clipType() = 0;
	/** Returns an iterator to the clip after the last clip(chronologically) which overlaps the
	start/end value range specified.

	This allows you to write standard iterator for loops over a specific range of the track.
	You must choose which list of tracks you are interested in - the selected or unselected. */
	QPtrListIterator<DocClipBase> endClip(GenTime startValue, GenTime endValue, bool selected);
	/** Returns an iterator to the first clip (chronologically) which overlaps the start/end 
	 * value range specified.
	 * You must choose which list of tracks you are interested in - the selected or unselected. */  
  QPtrListIterator<DocClipBase> firstClip(GenTime startValue, GenTime endValue, bool selected);
  /** Returns the clip which resides at the current value, or 0 if non exists */
  DocClipBase *getClipAt(GenTime value);
  /** Adds all of the clips in the pointerlist into this track. */
  void addClips(DocClipBaseList list, bool selected);
  /** returns true if all of the clips within the cliplist can be added, returns false otherwise. */
  bool canAddClips(DocClipBaseList clipList);;
  /** Returns true if the clip given exists in this track, otherwise returns
false. */
  bool clipExists(DocClipBase *clip);
  /** Removes the given clip from this track. If it doesn't exist, then
			a warning is issued vi kdWarning, but otherwise no bad things
			will happen. The clip is removed from the track but NOT deleted.
			Returns true on success, false on failure*/
  bool removeClip(DocClipBase *clip);
  /** The clip specified has moved. This method makes sure that the clips
are still in the correct order, rearranging them if they are not. */
  void clipMoved(DocClipBase *clip);
  /** Returns true if at least one clip in the track is selected, false otherwise. */
  bool hasSelectedClips();
  /** Returns an iterator to the first clip on the track.
You must choose which list of tracks you are interested in - the selected or unselected. */
  QPtrListIterator<DocClipBase> firstClip(bool selected) const;
  /** Moves either the selected or non selected clips by the specified time offset. */
  void moveClips(GenTime offset, bool selected);
  /** Removes either all selected clips, or all unselected clips and places them into a list. This list is then returned. */
  DocClipBaseList removeClips(bool selected);
  /** Deletes the clips in this trackbase. if selected is true, then all selected clips are deleted, otherwise all unselected clips are deleted. */
  void deleteClips(bool selected);
  /** Returns true if the clip is in the track and is selected, false if the clip
is not on the track, or if the clip is unselected. */
  bool clipSelected(DocClipBase *clip);
  /** Resizes this clip, using a new track start time of newStart.
   If this is not possible, it fails gracefully. */
  void resizeClipTrackStart(DocClipBase *clip, GenTime newStart);
  /** Resizes this clip, using a new cropDuration time of newStart.
   If this is not possible, it fails gracefully. */  
  void resizeClipTrackEnd(DocClipBase *clip, GenTime newStart);
  /** Returns the total length of the track - in other words, it returns the end of the
last clip on the track. */
  GenTime trackLength();
  /** Returns the number of clips contained in this track. */
  unsigned int numClips();
  /** Returns an xml representation of this track. */
  QDomDocument toXML();
  /** Returns the parent document of this track. */
  KdenliveDoc * document();
  /** Creates a track from the given xml document. Returns the track, or 0 if it could not be created. */
  static DocTrackBase * createTrack(KdenliveDoc *doc, QDomElement elem);
  /** Alerts the track that it's trackIndex within the document has
changed. The track should update the clips on it with the new
index value. */
  void trackIndexChanged(int index);
  /** Sets the specified clip to be in the specified selection state. returns false if the clip
  is not on the track. */
  bool selectClip(DocClipBase *clip, bool selected);
private: // Private methods
  /** Enables or disables clip sorting. This method is used internally to turn off the sorting of clips when it is known that they will be sorted elsewhere.

	If two disables are called, then two enables will be required to re-enable sorting.

	This method is dangerous, as it will mess up the data structure if sorting is disabled and never re-enabled. This method should never become a public API. */
	
  void enableClipSorting(bool enabled);
  /** If true, then upon adding or moving a clip, the track will ensure the clip is allowed to perform the move, and fail otherwise. If false, then this does not occur.

For internal use only, when the class Knows Better (TM) */
  void enableCollisionDetection(bool enable);  
public: // Public attributes
  /** Contains a list of all of the unselected clips within this track. */
  DocClipBaseList m_unselectedClipList;
  /** Contains a list of all of the selected clips within this track. */
  DocClipBaseList m_selectedClipList;
signals:
	/** Emitted whenever the clip layout changes.*/
	void clipLayoutChanged();
	/** Emitted whenever the clip selection.*/
	void clipSelectionChanged();  
  /** Emitted whenever a clip becomes selected. */
  void signalClipSelected(DocClipBase *);
private: // Private attributes
  /** This variable is >= 1 if sorting is enabled, and < 1 otherwise. */
  int m_sortingEnabled;
  /** This variable is >=1 if detection is enabled, and <1 otherwise. */
  int m_collisionDetectionEnabled;
  /** Holds a pointer to the document which contains this track. */
  KdenliveDoc *m_doc;
};

#endif
