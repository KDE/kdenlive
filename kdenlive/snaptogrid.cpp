/***************************************************************************
                          snaptogrid.cpp  -  description
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
#include "gentime.h"
#include "snaptogrid.h"
#include "qptrlist.h"
#include "qvaluelist.h"

SnapToGrid::SnapToGrid(KdenliveDoc *doc) :
		m_document(doc),
		m_isDirty(true)
{
	m_internalSnapTracker = m_internalSnapList.end();
}


SnapToGrid::~SnapToGrid()
{
}

/**
Add and set seek times. A seek time is a time that we wish to snap to, but
which is independant of the clip borders.
*/
uint SnapToGrid::addSeekTime(const GenTime &seekTime)
{
	m_seekTimes.append(seekTime);
	if(snapToSeekTime()) setDirty(true);
	return m_seekTimes.count()-1;
}

void SnapToGrid::setSeekTime(uint index, const GenTime &seekTime)
{
	if(index < m_seekTimes.count()) {
		m_seekTimes[index] = seekTime;
		if(snapToSeekTime()) setDirty(true);
	}
}

/**
get/set whether or not clip start times are snapped to
*/
void SnapToGrid::setSnapToClipStart(bool snapToClipStart)
{
	if(m_snapToClipStart != snapToClipStart) {
		m_snapToClipStart = snapToClipStart;
		setDirty(true);
	}
}

bool SnapToGrid::snapToClipStart() const
{
	return m_snapToClipStart;
}

void SnapToGrid::setIncludeSelectedClips(bool includeSelectedClips)
{
	if(m_includeSelectedClips != includeSelectedClips) {
		m_includeSelectedClips = includeSelectedClips;
		setDirty(true);
	}
}

bool SnapToGrid::includeSelectedClips() const
{
	return m_includeSelectedClips;
}

/**
get/set whether or not clip end times are snapped to
*/
void SnapToGrid::setSnapToClipEnd(bool snapToClipEnd)
{
	if(m_snapToClipEnd != snapToClipEnd) {
		m_snapToClipEnd = snapToClipEnd;
		setDirty(true);
	}
}

bool SnapToGrid::snapToClipEnd() const
{
	return m_snapToClipEnd;
}

/**
get/set whether or not seek times are snapped to
*/
void SnapToGrid::setSnapToSeekTime(bool snapToSeekTime)
{
	if(m_snapToSeekTime != snapToSeekTime) {
		m_snapToSeekTime = snapToSeekTime;
		setDirty(true);
	}
}

bool SnapToGrid::snapToSeekTime() const
{
	return m_snapToSeekTime;
}

/**
Get/set snapToFrame value. If true, then any returned time will snap to the
current frame.
*/
void SnapToGrid::setSnapToFrame(bool snapToFrame)
{
	if(m_snapToFrame != snapToFrame)
	{
		m_snapToFrame = snapToFrame;
		setDirty(true);
	}
}

bool SnapToGrid::snapToFrame() const
{
	return m_snapToFrame;
}

/**
Sets the cursor times to the specified list. Each cursor time is individually
'snapped' to the list of snap times when calculating the final snap.
*/
void SnapToGrid::setCursorTimes(const QValueList<GenTime> & time)
{
	m_cursorTimes = time;
}

void SnapToGrid::setDirty(bool dirty)
{
	m_isDirty = dirty;
}

bool SnapToGrid::isDirty() const
{
	return m_isDirty;
}

void SnapToGrid::setSnapTolerance(const GenTime &tolerance)
{
	m_snapTolerance = tolerance;
}

const GenTime &SnapToGrid::snapTolerance() const
{
	return m_snapTolerance;
}

/**
Returns a snapped time compared to the specified GenTime. In the case of their being
multiple cursors, it is assumed that the time specified is that which the first
cursor time in the list will move to. The returned time takes into account clip
begin/end times (if they are turned on), seek times (if they are turned on), and
frame borders (if they are turned on).
*/
GenTime SnapToGrid::getSnappedTime(const GenTime &time) const
{
	GenTime result = time;

	if(isDirty()) {
		createInternalSnapList();
		m_isDirty = false;
	}

	if(m_internalSnapTracker != m_internalSnapList.end()) {
		QValueListConstIterator<GenTime> itt = m_internalSnapTracker;
		++itt;
		while(itt != m_internalSnapList.end()) {
			double newTime = fabs(((*itt) - time).seconds());
			double oldTime = fabs(((*m_internalSnapTracker) - time).seconds());
			if ( newTime > oldTime ) break;

			m_internalSnapTracker = itt;
			++itt;
		}

		itt = m_internalSnapTracker;
		--itt;
		while(m_internalSnapTracker != m_internalSnapList.begin()) {
			double newTime = fabs(((*itt) - time).seconds());
			double oldTime = fabs(((*m_internalSnapTracker) - time).seconds());
			if(newTime > oldTime) break;

			m_internalSnapTracker = itt;
			--itt;
		}

		double diff = fabs(((*m_internalSnapTracker) - result).seconds());
		if(diff < m_snapTolerance.seconds()) {
			result = *m_internalSnapTracker;
		}
	}

	if(m_snapToFrame) {
		result = GenTime((int)floor(result.frames(m_document->framesPerSecond())+0.5), m_document->framesPerSecond());
	}

	return result;
}

void SnapToGrid::createInternalSnapList() const
{
	m_internalSnapList.clear();
	m_internalSnapTracker = m_internalSnapList.begin();
	if(m_cursorTimes.count() == 0) return;

	QValueList<GenTime> list = snapToGridList();

	// For each cursor time, append a correct snap time to the list.
	// This is calculated as follows : masterClipTime + required snap time - cursor time.

	QValueListConstIterator<GenTime> cursorItt = m_cursorTimes.begin();
	while(cursorItt != m_cursorTimes.end())
	{
		QValueListIterator<GenTime> timeItt = list.begin();

		while(timeItt != list.end()) {
			m_internalSnapList.append(m_cursorTimes[0] + (*timeItt) - (*cursorItt));
			++timeItt;
		}

		++cursorItt;
	}


  qHeapSort(m_internalSnapList);

  QValueListIterator<GenTime> itt = m_internalSnapList.begin();
  if(itt != m_internalSnapList.end()) {
	 	QValueListIterator<GenTime> next = itt;
	  ++next;

	  while(next != m_internalSnapList.end()) {
	  	if((*itt) == (*next)) {
	   		m_internalSnapList.remove(next);
	   		next = itt;
				++next;
			} else {
				++itt;
				++next;
			}
	  }
	}

  m_internalSnapTracker = m_internalSnapList.begin();
}

QValueList<GenTime> SnapToGrid::snapToGridList() const
{
	QValueList<GenTime> list;

	// Add snap to seek positions.
	if(m_snapToSeekTime) {
		list = m_seekTimes;
	}

	// Add all trackStart/trackEnd points to the list as appropriate.

	for(uint count=0; count<m_document->numTracks(); ++count)
	{
		QPtrListIterator<DocClipBase> clipItt = m_document->track(count)->firstClip(false);
		while(clipItt.current()) {
			if(m_snapToClipStart) list.append(clipItt.current()->trackStart());
			if(m_snapToClipEnd) list.append(clipItt.current()->trackEnd());
			++clipItt;
		}

		if(m_includeSelectedClips) {
			clipItt = m_document->track(count)->firstClip(true);
			while(clipItt.current()) {
				if(m_snapToClipStart) list.append(clipItt.current()->trackStart());
				if(m_snapToClipEnd) list.append(clipItt.current()->trackEnd());
				++clipItt;
			}
		}
	}

	return list;
}

void SnapToGrid::prependCursor(const GenTime &cursor)
{
	m_cursorTimes.prepend(cursor);
	setDirty(true);
}

void SnapToGrid::appendCursor(const GenTime &cursor)
{
	m_cursorTimes.append(cursor);
	setDirty(true);
}

void SnapToGrid::clearSeekTimes()
{
	m_seekTimes.clear();
	if(m_snapToSeekTime) {
		setDirty(true);
	}
}
