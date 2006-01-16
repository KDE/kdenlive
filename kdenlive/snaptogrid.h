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
class SnapToGrid {
  public:
    SnapToGrid();

    ~SnapToGrid();

	/**
	Sets the cursor times to the specified list. Each cursor time is individually
	'snapped' to the list of snap times when calculating the final snap.
	*/
    void setCursorTimes(const QValueVector < GenTime > &time);

	/**
	Adds a new cursor time at the beginning of the cursor list.
	*/
    void prependCursor(const GenTime & cursor);

	/**
	Adds a new cursor time at the end of the cursor list.
	*/
    void appendCursor(const GenTime & cursor);

	/**
	Get/Set the snap tolerance. This is the time interval that we must be withing before
	we snap to a a given point.
	*/
    void setSnapTolerance(const GenTime & tolerance);
    const GenTime & snapTolerance() const;

    double framesPerSecond() const {
	return m_framesPerSecond;
    } void setFramesPerSecond(double fps) {
	m_framesPerSecond = fps;
    }
	/**
	Returns a snapped time compared to the specified GenTime. In the case of their being
	multiple cursors, it is assumed that the time specified is that which the first
	cursor time in the list will move to. The returned time takes into account clip
	begin/end times (if they are turned on), seek times (if they are turned on), and
	frame borders (if they are turned on).
	*/ GenTime getSnappedTime(const GenTime & time) const;

    // Adds new times to the list of points we snap to.
    void addToSnapList(const GenTime & time);
    void addToSnapList(const QValueVector < GenTime > &times);

    // Clear the snap list.
    void clearSnapList();

	/**
	Get/set snapToFrame value. If true, then any returned time will snap to the
	current frame.
	*/
    void setSnapToFrame(bool snapToFrame);
    bool snapToFrame() const;
  private:
    QValueList < GenTime > m_seekTimes;
    QValueVector < GenTime > m_cursorTimes;

    bool m_snapToFrame;

    GenTime m_snapTolerance;

	/** A list of all times that are "snapToGrid" times. Generated specifically for a
	particular set of cursor times, this list of times is the only thing we need to
	consult when checkinga time to determine when we need to snap to a particular
	location based upon another	clip. */
    mutable QValueList < GenTime > m_internalSnapList;
	/** Keeps track of whichever list item is closest to the mouse cursor. */
    mutable QValueListConstIterator < GenTime > m_internalSnapTracker;
    mutable bool m_isDirty;

	/** A list of all snap points */
    QValueList < GenTime > m_snapToGridList;

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
    QValueList < GenTime > snapToGridList()const;

	/** Generates the internal snap list. See m_internalSnapList for more details.*/
    void createInternalSnapList() const;

	/** If we snap to frames per second, this is the number of seconds per frame. */
    double m_framesPerSecond;
};

#endif
