/***************************************************************************
                          trackpanelfunction.cpp  -  description
                             -------------------
    begin                : Sun May 18 2003
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
#include "trackpanelspacerfunction.h"

#include "doctrackbase.h"
#include "kmmtimeline.h"
#include "kdenlivedoc.h"
#include "kmoveclipscommand.h"

TrackPanelSpacerFunction::TrackPanelSpacerFunction(KMMTimeLine *timeline,
							DocTrackBase *docTrack,
							KdenliveDoc *doc) :
						m_timeline(timeline),
						m_docTrack(docTrack),
						m_doc(doc),
						m_masterClip(0),
						m_moveClipsCommand(0),
						m_snapToGrid(doc)
{
}


TrackPanelSpacerFunction::~TrackPanelSpacerFunction()
{
}

bool TrackPanelSpacerFunction::mouseApplies(QMouseEvent *event) const
{
	return true;
}

QCursor TrackPanelSpacerFunction::getMouseCursor(QMouseEvent *event)
{
	return QCursor(Qt::SizeHorCursor);
}

bool TrackPanelSpacerFunction::mousePressed(QMouseEvent *event)
{
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_doc->framesPerSecond());
	GenTime roundedMouseTime = m_timeline->timeUnderMouse(event->x());
	m_clipUnderMouse = 0;
	
	if (event->state() & ShiftButton) {
		m_timeline->addCommand(m_timeline->selectLaterClips(mouseTime, false), true);
	} else {
		m_timeline->addCommand(m_timeline->selectLaterClips(mouseTime, true), true);
	}
	
	if(m_doc->hasSelectedClips()) {
		m_masterClip = m_doc->selectedClip();
		m_moveClipsCommand = new Command::KMoveClipsCommand(m_timeline, m_doc, m_masterClip);
		m_clipOffset = mouseTime - m_masterClip->trackStart();
		
		m_snapToGrid.setSnapToClipStart(m_timeline->snapToBorders());
		m_snapToGrid.setSnapToClipEnd(m_timeline->snapToBorders());
		m_snapToGrid.setSnapToFrame(m_timeline->snapToFrame());
		m_snapToGrid.setSnapToSeekTime(m_timeline->snapToSeekTime());
		m_snapToGrid.setSnapTolerance(GenTime(m_timeline->mapLocalToValue(KMMTimeLine::snapTolerance) - m_timeline->mapLocalToValue(0), m_doc->framesPerSecond()));
		m_snapToGrid.setIncludeSelectedClips(false);
		m_snapToGrid.clearSeekTimes();
		m_snapToGrid.addSeekTime(m_timeline->seekPosition());
		
		QValueList<GenTime> cursor = m_timeline->selectedClipTimes();
		m_snapToGrid.setCursorTimes(cursor);
	}
	
	return true;
}

bool TrackPanelSpacerFunction::mouseReleased(QMouseEvent *event)
{
	if(m_moveClipsCommand) {
		m_moveClipsCommand->setEndLocation(m_masterClip);
		m_timeline->addCommand(m_moveClipsCommand, false);
		m_moveClipsCommand = 0;
	}
		
	return true;
}

bool TrackPanelSpacerFunction::mouseMoved(QMouseEvent *event)
{
	GenTime mouseTime = m_timeline->timeUnderMouse(event->x()) - m_clipOffset;
	mouseTime = m_snapToGrid.getSnappedTime(mouseTime);
	mouseTime = mouseTime + m_clipOffset;
	
	if(m_moveClipsCommand) {
		GenTime startOffset = mouseTime - m_clipOffset - m_masterClip->trackStart();
		m_doc->moveSelectedClips(startOffset, 0);
		m_timeline->drawTrackViewBackBuffer();
	}
	
	return true;
}


