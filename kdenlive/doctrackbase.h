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

#include "docclipreflist.h"

class QString;

class DocClipProject;
class ClipManager;
class ClipProject;

/**DocTrackBase is a base class for all real track entries into the database.
It provides core functionality that all tracks must possess.
  *@author Jason Wood
  */

class DocTrackBase:public QObject {
  Q_OBJECT public:
    DocTrackBase(DocClipProject * project);
    virtual ~ DocTrackBase();
	/** Returns true if the specified clip can be added to this track, false otherwise.
	*
	* This method needs to be implemented by inheriting classes to define
	* which types of clip they support. */
    virtual bool canAddClip(DocClipRef * clip) const = 0;
	/** Adds a clip to the track. Returns true if successful, false if it fails for some reason.
	* This method calls canAddClip() to determine whether or not the clip can be added to this
	* particular track. */
    bool addClip(DocClipRef * clip, bool selected);
	/** Returns the clip type as a string. This is a bit of a hack to give the
	* KMMTimeLine a way to determine which class it should associate
	*	with each type of clip. */
    virtual const QString & clipType() const = 0;
	/** Returns an iterator to the clip after the last clip(chronologically) which overlaps the
	start/end value range specified.

	This allows you to write standard iterator for loops over a specific range of the track.
	You must choose which list of tracks you are interested in - the selected or unselected. */
     QPtrListIterator < DocClipRef > endClip(GenTime startValue,
	GenTime endValue, bool selected);
	/** Returns an iterator to the first clip (chronologically) which overlaps the start/end
	 * value range specified.
	 * You must choose which list of tracks you are interested in - the selected or unselected. */
     QPtrListIterator < DocClipRef > firstClip(GenTime startValue,
	GenTime endValue, bool selected);
	/** Returns the clip which resides at the current value, or 0 if non exists */
    DocClipRef *getClipAt(const GenTime & value) const;
	/** Adds all of the clips in the pointerlist into this track. */
    void addClips(DocClipRefList list, bool selected);
	/** returns true if all of the clips within the cliplist can be added, returns false otherwise. */
    bool canAddClips(DocClipRefList clipList);
	/** Returns true if the clip given exists in this track, otherwise returns
	false. */
    bool clipExists(DocClipRef * clip);
	/** Removes the given clip from this track. If it doesn't exist, then
			a warning is issued vi kdWarning, but otherwise no bad things
			will happen. The clip is removed from the track but NOT deleted.
			Returns true on success, false on failure*/
    bool removeClip(DocClipRef * clip);
	/** The clip specified has moved. This method makes sure that the clips
	are still in the correct order, rearranging them if they are not. */
    void clipMoved(DocClipRef * clip);
	/** Returns true if at least one clip in the track is selected, false otherwise. */
    int hasSelectedClips();
	/** Returns an iterator to the first clip on the track.
	You must choose which list of tracks you are interested in - the selected or unselected. */
     QPtrListIterator < DocClipRef > firstClip(bool selected) const;
	/** Moves either the selected or non selected clips by the specified time offset. */
    void moveClips(GenTime offset, bool selected);
	/** Removes either all selected clips, or all unselected clips and places them into a list. This list is then returned. */
    DocClipRefList removeClips(bool selected);
	/** Deletes the clips in this trackbase. if selected is true, then all selected clips are deleted, otherwise all unselected clips are deleted. */
    void deleteClips(bool selected);
	/** Returns true if the clip is in the track and is selected, false if the clip
	is not on the track, or if the clip is unselected. */
    bool clipSelected(DocClipRef * clip) const;
	/** Resizes this clip, using a new track start time of newStart.
	If this is not possible, it fails gracefully. */
    void resizeClipTrackStart(DocClipRef * clip, GenTime newStart);
	/** Resizes this clip, using a new cropDuration time of newStart.
	If this is not possible, it fails gracefully. */
    void resizeClipTrackEnd(DocClipRef * clip, GenTime newStart);
	/** Returns the total length of the track - in other words, it returns the end of the
	last clip on the track. */
    const GenTime & trackLength() const;
	/** Returns the number of clips contained in this track. */
    unsigned int numClips() const;
	/** Returns an xml representation of this track. */
    QDomDocument toXML();
	/** Returns true if the QDomElement passed matches the contents of this track */
    bool matchesXML(const QDomElement & element) const;
	/** Creates a track from the given xml document. Returns the track, or 0 if it
	 * could not be created. */
    static DocTrackBase *createTrack(const EffectDescriptionList &
	descList, ClipManager & clipManager, DocClipProject * project,
	QDomElement elem);
	/** Alerts the track that it's trackIndex within the document has
	changed. The track should update the clips on it with the new
	index value. */
    void trackIndexChanged(int index);
	/** Sets the specified clip to be in the specified selection state. returns false if the clip
	is not on the track. */
    bool selectClip(DocClipRef * clip, bool selected);

    bool openClip(DocClipRef * clip);

	/** Returns true if the clip is referenced by at least one clip on the track. */
    bool referencesClip(DocClipBase * clip) const;

	/** Returns a list of clips on this track that reference the specified clip. */
    DocClipRefList referencedClips(DocClipBase * clip) const;

	/** Returns the number of frames per second this track should play at. */
    double framesPerSecond() const;

	/** Called by the clip in question to alert doctrackbase that the clip has changed. */
    void notifyClipChanged(DocClipRef * clip);

	/** Checks to see if the track length has changed since it was last calculated, and if it has, emit a
	"track length changed" signal. */
    void checkTrackLength();

	/** Add a new effect to a clip on the track, and emit a signal. */
    void addEffectToClip(const GenTime & position, int effectIndex,
	Effect * effect);

	/** Delete an effect from a clip on the track and emit a signal. */
    void deleteEffectFromClip(const GenTime & position, int effectIndex);
    
        /** Return a pointer to the project */
    DocClipProject * projectClip();
    
    bool isMute();
    void mute(bool muted);
    bool isBlind();
    void blind(bool blinded);

  public slots:
        /** Redraw the tracks view */
    void refreshLayout();


  private:			// Private methods
	/** Enables or disables clip sorting. This method is used internally to turn off the sorting of clips when it is known that they will be sorted elsewhere.

	If two disables are called, then two enables will be required to re-enable sorting.

	This method is dangerous, as it will mess up the data structure if sorting is disabled and never re-enabled. This method should never become a public API. */

    void enableClipSorting(bool enabled);
	/** If true, then upon adding or moving a clip, the track will ensure the clip is allowed to perform the move, and fail otherwise. If false, then this does not occur.

For internal use only, when the class Knows Better (TM) */
    void enableCollisionDetection(bool enable);
     
    
    signals:
	/** Emitted whenever the clip layout changes.*/
    void clipLayoutChanged();
	/** Emitted whenever the clip selection.*/
    void clipSelectionChanged();
	/** Emitted whenever a clip becomes selected. */
    void signalClipSelected(DocClipRef *);
    void signalOpenClip(DocClipRef *);
	/** Emitted whenever a clip changes in some way, for example, gains or loses snapMarkers. */
    void clipChanged(DocClipRef *);
	/** Emitted if the length of the track changes. */
    void trackLengthChanged(const GenTime &);
	/** Emitted when the specified clips effectStack changes. */
    void effectStackChanged(DocClipRef *);
  protected:			// Protected attributes
	/** Contains a list of all of the unselected clips within this track. */
     DocClipRefList m_unselectedClipList;
	/** Contains a list of all of the selected clips within this track. */
    DocClipRefList m_selectedClipList;
  private:			// Private attributes
	/** This variable is >= 1 if sorting is enabled, and < 1 otherwise. */
    int m_sortingEnabled;
	/** This variable is >=1 if detection is enabled, and <1 otherwise. */
    int m_collisionDetectionEnabled;
	/** Holds a pointer to the clip project which contains this track. */
    DocClipProject *m_project;
    
    bool m_mute;
    bool m_blind;

    GenTime m_trackLength;
};

#endif
