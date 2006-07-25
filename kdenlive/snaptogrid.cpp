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

#include "kdenlivesettings.h"

SnapToGrid::SnapToGrid():
m_snapToFrame(true), m_isDirty(true), m_framesPerSecond(KdenliveSettings::defaultfps())
{
    m_internalSnapTracker = m_internalSnapList.end();
}


SnapToGrid::~SnapToGrid()
{
}

/**
Get/set snapToFrame value. If true, then any returned time will snap to the
current frame.
*/
void SnapToGrid::setSnapToFrame(bool snapToFrame)
{
    if (m_snapToFrame != snapToFrame) {
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
void SnapToGrid::setCursorTimes(const QValueVector < GenTime > &time)
{
    m_cursorTimes = time;
    setDirty(true);
}

void SnapToGrid::setDirty(bool dirty)
{
    m_isDirty = dirty;
}

bool SnapToGrid::isDirty() const
{
    return m_isDirty;
}

void SnapToGrid::setSnapTolerance(const GenTime & tolerance)
{
    m_snapTolerance = tolerance;
}

const GenTime & SnapToGrid::snapTolerance() const
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
GenTime SnapToGrid::getSnappedTime(const GenTime & time) const
{
    GenTime result = time;

    if (isDirty()) {
	createInternalSnapList();
	m_isDirty = false;
    }

    if (m_internalSnapTracker != m_internalSnapList.end()) {
	QValueListConstIterator < GenTime > itt = m_internalSnapTracker;
	++itt;
	while (itt != m_internalSnapList.end()) {
	    double newTime = fabs(((*itt) - time).seconds());
	    double oldTime =
		fabs(((*m_internalSnapTracker) - time).seconds());
	    if (newTime > oldTime)
		break;

	    m_internalSnapTracker = itt;
	    ++itt;
	}

	itt = m_internalSnapTracker;
	--itt;
	while (m_internalSnapTracker != m_internalSnapList.begin()) {
	    double newTime = fabs(((*itt) - time).seconds());
	    double oldTime =
		fabs(((*m_internalSnapTracker) - time).seconds());
	    if (newTime > oldTime)
		break;

	    m_internalSnapTracker = itt;
	    --itt;
	}

	double diff = fabs(((*m_internalSnapTracker) - result).seconds());
	if (diff < m_snapTolerance.seconds()) {
	    result = *m_internalSnapTracker;
	}
    }

    if (m_snapToFrame) {
	result.roundNearestFrame(m_framesPerSecond);
    }

    return result;
}

void SnapToGrid::createInternalSnapList() const
{
    m_internalSnapList.clear();
    m_internalSnapTracker = m_internalSnapList.begin();
    if (m_cursorTimes.count() == 0)
	return;

    QValueList < GenTime > list = snapToGridList();

    // For each cursor time, append a correct snap time to the list.
    // This is calculated as follows : masterClipTime + required snap time - cursor time.

    QValueVector < GenTime >::const_iterator cursorItt =
	m_cursorTimes.begin();
    while (cursorItt != m_cursorTimes.end()) {
	QValueListIterator < GenTime > timeItt = list.begin();

	while (timeItt != list.end()) {
	    m_internalSnapList.append(m_cursorTimes[0] + (*timeItt) -
		(*cursorItt));
	    ++timeItt;
	}

	++cursorItt;
    }


    qHeapSort(m_internalSnapList);

    QValueListIterator < GenTime > itt = m_internalSnapList.begin();
    if (itt != m_internalSnapList.end()) {
	QValueListIterator < GenTime > next = itt;
	++next;

	while (next != m_internalSnapList.end()) {
	    if ((*itt) == (*next)) {
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

void SnapToGrid::clearSnapList()
{
    m_snapToGridList.clear();
}

void SnapToGrid::addToSnapList(const GenTime & time)
{
    m_snapToGridList.append(time);
    setDirty(true);
}

void SnapToGrid::addToSnapList(const QValueVector < GenTime > &times)
{
    for (QValueVector < GenTime >::const_iterator itt = times.begin();
	itt != times.end(); ++itt) {
	m_snapToGridList.append(*itt);
    }
    setDirty(true);
}

QValueList < GenTime > SnapToGrid::snapToGridList() const
{
    return m_snapToGridList;
}

	/**QValueList<GenTime> list;

	// Add snap to seek positions.
	if(m_snapToSeekTime) {
		list = m_seekTimes;
	}

	// Add all trackStart/trackEnd points to the list as appropriate.

	for(uint count=0; count<m_document->numTracks(); ++count)
	{
		QPtrListIterator<DocClipRef> clipItt = m_document->track(count)->firstClip(false);
		while(clipItt.current()) {
			if(m_snapToClipStart) list.append(clipItt.current()->trackStart());
			if(m_snapToClipEnd) list.append(clipItt.current()->trackEnd());

			if(m_snapToMarkers) {
				QValueVector<GenTime> markers = clipItt.current()->snapMarkersOnTrack();
				for(uint count=0; count<markers.count(); ++count) {
					list.append(markers[count]);
				}
			}

			++clipItt;
		}

		if(m_includeSelectedClips) {
			clipItt = m_document->track(count)->firstClip(true);
			while(clipItt.current()) {
				if(m_snapToClipStart) list.append(clipItt.current()->trackStart());
				if(m_snapToClipEnd) list.append(clipItt.current()->trackEnd());

				if(m_snapToMarkers) {
					QValueVector<GenTime> markers = clipItt.current()->snapMarkersOnTrack();
					for(uint count=0; count<markers.count(); ++count) {
						list.append(markers[count]);
					}
				}

				++clipItt;
			}
		}
	}

	return list;
}*/

void SnapToGrid::prependCursor(const GenTime & cursor)
{
    m_cursorTimes.insert(0, cursor);
    setDirty(true);
}

void SnapToGrid::appendCursor(const GenTime & cursor)
{
    m_cursorTimes.append(cursor);
    setDirty(true);
}
