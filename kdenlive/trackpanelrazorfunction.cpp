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
#include "trackpanelrazorfunction.h"

#include "docclipbase.h"
#include "kmmtimeline.h"
	
TrackPanelRazorFunction::TrackPanelRazorFunction(KMMTimeLine *timeline, 
							KdenliveDoc *document,
							DocTrackBase *docTrack) :
								m_timeline(timeline),
								m_document(document),
								m_docTrack(docTrack),
								m_clipUnderMouse(0)
{
}


TrackPanelRazorFunction::~TrackPanelRazorFunction()
{
}

bool TrackPanelRazorFunction::mouseApplies(QMouseEvent *event) const
{
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	DocClipRef *clip = m_docTrack->getClipAt(mouseTime);
	return clip;
}

QCursor TrackPanelRazorFunction::getMouseCursor(QMouseEvent *event)
{
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	DocClipRef *clip = m_docTrack->getClipAt(mouseTime);
	if(clip) {
		// emit lookingAtClip(clip, mouseTime - clip->trackStart() + clip->cropStartTime());
	}	
	return QCursor(Qt::SplitVCursor);
}

bool TrackPanelRazorFunction::mousePressed(QMouseEvent *event)
{
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	GenTime roundedMouseTime = m_timeline->timeUnderMouse(event->x());

	m_clipUnderMouse = m_docTrack->getClipAt(mouseTime);
	if(m_clipUnderMouse) {
		if (event->state() & ShiftButton) {
			m_timeline->razorAllClipsAt(roundedMouseTime);
		} else {
			m_timeline->addCommand(m_timeline->razorClipAt(*m_docTrack, roundedMouseTime), true);
		}
		return true;
	}
	return true;
}

bool TrackPanelRazorFunction::mouseReleased(QMouseEvent *event)
{
 	GenTime mouseTime = m_timeline->timeUnderMouse(event->x());
	m_clipUnderMouse = 0;
	return true;
}

bool TrackPanelRazorFunction::mouseMoved(QMouseEvent *event) 
{
	return true;
}
