/***************************************************************************
                          trackpanelmarkerfunction  -  description
                             -------------------
    begin                : Thu Nov 27 2003
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
#include "trackpanelmarkerfunction.h"

#include "kaddmarkercommand.h"
#include "kmmtimeline.h"

TrackPanelMarkerFunction::TrackPanelMarkerFunction(KMMTimeLine *timeline, DocTrackBase *docTrack, KdenliveDoc *document) :
									m_timeline(timeline),
									m_docTrack(docTrack),
									m_document(document)
{
}


TrackPanelMarkerFunction::~TrackPanelMarkerFunction()
{
}


bool TrackPanelMarkerFunction::mouseApplies(QMouseEvent* event) const
{
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	DocClipRef *clipUnderMouse = 0;
	clipUnderMouse = m_docTrack->getClipAt(mouseTime);

	return clipUnderMouse;
}

bool TrackPanelMarkerFunction::mouseMoved(QMouseEvent* event)
{
	return true;
}

bool TrackPanelMarkerFunction::mousePressed(QMouseEvent* event)
{
	return true;
}

bool TrackPanelMarkerFunction::mouseReleased(QMouseEvent* event)
{
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	DocClipRef *clipUnderMouse = 0;
	clipUnderMouse = m_docTrack->getClipAt(mouseTime);

	if(clipUnderMouse) {
		Command::KAddMarkerCommand *command = new Command::KAddMarkerCommand(*m_document, clipUnderMouse, mouseTime, true);
		m_timeline->addCommand(command);
	}

	return true;
}

QCursor TrackPanelMarkerFunction::getMouseCursor(QMouseEvent* event)
{
	return QCursor(Qt::PointingHandCursor);
}

#include "trackpanelmarkerfunction.moc"
