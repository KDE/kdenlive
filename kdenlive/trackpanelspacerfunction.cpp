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

TrackPanelSpacerFunction::TrackPanelSpacerFunction(KMMTimeLine *timeline, DocTrackBase *docTrack) :
								m_timeline(timeline),
								m_docTrack(docTrack)
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
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_docTrack->document()->framesPerSecond());
	GenTime roundedMouseTime = m_timeline->timeUnderMouse(event->x());
	m_clipUnderMouse = 0;
	
	if (event->state() & ShiftButton) {
		m_timeline->addCommand(m_timeline->selectLaterClips(mouseTime, true), true);
	} else {
		m_timeline->addCommand(m_timeline->selectLaterClips(mouseTime, false), true);
	}
	return true;
}

bool TrackPanelSpacerFunction::mouseReleased(QMouseEvent *event)
{
	return true;
}

bool TrackPanelSpacerFunction::mouseMoved(QMouseEvent *event)
{
	return true;
}
