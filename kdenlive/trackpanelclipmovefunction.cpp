/***************************************************************************
                          TrackPanelClipMoveFunction.cpp  -  description
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
#include "trackpanelclipmovefunction.h"

#include "qnamespace.h"

#include "doctrackbase.h"
#include "kmmtimeline.h"

TrackPanelClipMoveFunction::TrackPanelClipMoveFunction(KMMTimeLine *timeline, 
								KdenliveDoc *document,
								DocTrackBase *docTrack) :
								m_timeline(timeline),
								m_document(document),
								m_docTrack(docTrack)
{
}


TrackPanelClipMoveFunction::~TrackPanelClipMoveFunction()
{
}

bool TrackPanelClipMoveFunction::mouseApplies(QMouseEvent *event) const
{
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	DocClipRef *clipUnderMouse = 0;
	clipUnderMouse = m_docTrack->getClipAt(mouseTime);

	return clipUnderMouse;
}

QCursor TrackPanelClipMoveFunction::getMouseCursor(QMouseEvent *event)
{
	return QCursor(Qt::ArrowCursor);
}

bool TrackPanelClipMoveFunction::mousePressed(QMouseEvent *event)
{
	bool result = false;

	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	m_clipUnderMouse = 0;
	m_clipUnderMouse = m_docTrack->getClipAt(mouseTime);

	if(m_clipUnderMouse) {
		if (event->state() & Qt::ControlButton) {
	  		m_timeline->toggleSelectClipAt(*m_docTrack, mouseTime);
		} else if(event->state() & Qt::ShiftButton) {
		  	m_timeline->selectClipAt(*m_docTrack, mouseTime);
		}
		result = true;
	}

	return result;
}

bool TrackPanelClipMoveFunction::mouseReleased(QMouseEvent *event)
{
	bool result = false;

	if(m_clipUnderMouse) {
		if (event->state() & Qt::ControlButton) {
		} else if(event->state() & Qt::ShiftButton) {
		} else {
			GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
			m_timeline->addCommand(m_timeline->selectNone(), true);
			m_timeline->selectClipAt(*m_docTrack, mouseTime);
		}
		result = true;
	}

	return result;
}

bool TrackPanelClipMoveFunction::mouseMoved(QMouseEvent *event)
{
	bool result = false;

	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());

	if(m_dragging) {
		m_dragging = false;
		result = true;
	} else {
		if(m_clipUnderMouse) {
			if(!m_timeline->clipSelected(m_clipUnderMouse)) {
				if ((event->state() & Qt::ControlButton) || (event->state() & Qt::ShiftButton)) {
					m_timeline->selectClipAt(*m_docTrack,mouseTime);
				} else {
					m_timeline->addCommand(m_timeline->selectNone(), true);
					m_timeline->selectClipAt(*m_docTrack,mouseTime);
				}
			}
			m_dragging = true;
			m_timeline->initiateDrag(m_clipUnderMouse, mouseTime);

			result = true;
		}
	}

	return result;
}


