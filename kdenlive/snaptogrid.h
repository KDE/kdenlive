/***************************************************************************
                          snaptogrid.h  -  description
                             -------------------
    begin                : Fri May 9 2003
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
#ifndef SNAPTOGRID_H
#define SNAPTOGRID_H

#include <kdenlivedoc.h>

/**
Manages snap to grid times. Allows the getting and setting of various times
including seek positions, clip start and end times, and various others.

@author Jason Wood
*/
class SnapToGrid
{
public:
    SnapToGrid(KdenliveDoc *doc);

    ~SnapToGrid();

	/**
	Add and set seek times. A seek time is a time that we wish to snap to, but
	which is independant of the clip borders.
	*/
	uint addSeekTime(const GenTime &seekTime);
	void setSeekTime(uint index, const GenTime &seekTime);

	/** Clear all seek times */
	void clearSeekTimes();

	/**
	get/set whether or not clip start times are snapped to
	*/
	void setSnapToClipStart(bool snapToClipStart);
	bool snapToClipStart() const;

	/**
	get/set whether or not clip end times are snapped to
	*/
	void setSnapToClipEnd(bool snapToClipEnd);
	bool snapToClipEnd() const;

	/**
	get/set whether or not selected clips are included with unselected clips.
	*/
	void setIncludeSelectedClips(bool includeSelectedClips);
	bool includeSelectedClips() const;

	/**
	get/set whether or not seek times are snapped to
	*/
	void setSnapToSeekTime(bool snapToSeekTime);
	bool snapToSeekTime() const;

	/**
	Get/set snapToFrame value. If true, then any returned time will snap to the
	current frame.
	*/
	void setSnapToFrame(bool snapToFrame);
	bool snapToFrame() const;

	/* Get/set the snapToMarkers value. If true, any returned time will snap to
	the snapMarkers associated with clips.*/
	void setSnapToMarkers(bool snapToMarkers);
	bool snapToMarkers() const;

	/**
	Sets the cursor times to the specified list. Each cursor time is individually
	'snapped' to the list of snap times when calculating the final snap.
	*/
	void setCursorTimes(const QValueList<GenTime> & time);

	/**
	Adds a new cursor time at the beginning of the cursor list.
	*/
	void prependCursor(const GenTime &cursor);

	/**
	Adds a new cursor time at the end of the cursor list.
	*/
	void appendCursor(const GenTime &cursor);

	/**
	Get/Set the snap tolerance. This is the time interval that we must be withing before
	we snap to a a given point.
	*/
	void setSnapTolerance(const GenTime &tolerance);
	const GenTime &snapTolerance() const;

	/**
	Returns a snapped time compared to the specified GenTime. In the case of their being
	multiple cursors, it is assumed that the time specified is that which the first
	cursor time in the list will move to. The returned time takes into account clip
	begin/end times (if they are turned on), seek times (if they are turned on), and
	frame borders (if they are turned on).
	*/
	GenTime getSnappedTime(const GenTime &time) const;
private:
	KdenliveDoc *m_document;
	QValueList<GenTime> m_seekTimes;
	QValueList<GenTime> m_cursorTimes;

	bool m_snapToFrame;
	bool m_snapToClipStart;
	bool m_snapToClipEnd;
	bool m_includeSelectedClips;
	bool m_snapToSeekTime;
	bool m_snapToMarkers;

	GenTime m_snapTolerance;

 	/** A list of all times that are "snapToGrid" times. Generated specifically for a
	particular set of cursor times, this list of times is the only thing we need to
	consult when checkinga time to determine when we need to snap to a particular
	location based upon another	clip. */
  	mutable QValueList<GenTime> m_internalSnapList;
  	/** Keeps track of whichever list item is closest to the mouse cursor. */
  	mutable QValueListConstIterator<GenTime> m_internalSnapTracker;
	mutable bool m_isDirty;

private: // methods
	/**
	For efficiency reason, we only want to update the internal snap lists when it is
	absolutely necessary. So we keep track of when an important change has happened,
	and then only update the snap list when the next request for data is made.
	*/
	void setDirty(bool dirty);
	bool isDirty() const;

	/** Creates a list of all snap points for the timeline. This includes the current seek
	position, the beginning of the timeline, and the start and end points of every clip in
	the document */
	QValueList<GenTime> snapToGridList() const;

	/** Generates the internal snap list. See m_snapToGridList for more details.*/
	void createInternalSnapList() const;
};

#endif
