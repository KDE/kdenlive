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
#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"

TrackPanelMarkerFunction::TrackPanelMarkerFunction(Gui::KdenliveApp * app, Gui::KTimeLine * timeline, KdenliveDoc * document):
m_app(app), m_timeline(timeline), m_document(document)
{
}


TrackPanelMarkerFunction::~TrackPanelMarkerFunction()
{
}


bool TrackPanelMarkerFunction::mouseApplies(Gui::KTrackPanel * panel,
    QMouseEvent * event) const
{
    DocClipRef *clipUnderMouse = 0;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime(m_timeline->mapLocalToValue(event->x()),
		m_document->framesPerSecond());
	    clipUnderMouse = track->getClipAt(mouseTime);
	}
    }

    return clipUnderMouse;
}

bool TrackPanelMarkerFunction::mouseMoved(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    return true;
}

bool TrackPanelMarkerFunction::mousePressed(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    return true;
}

bool TrackPanelMarkerFunction::mouseReleased(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    DocClipRef *clipUnderMouse = 0;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime(m_timeline->mapLocalToValue(event->x()),
		m_document->framesPerSecond());
	    clipUnderMouse = track->getClipAt(mouseTime);

	    if (clipUnderMouse) {
		Command::KAddMarkerCommand * command =
		    new Command::KAddMarkerCommand(*m_document,
		    clipUnderMouse,
		    mouseTime - clipUnderMouse->trackStart() +
		    clipUnderMouse->cropStartTime(), true);
		m_app->addCommand(command);
	    }
	}
    }

    return true;
}

QCursor TrackPanelMarkerFunction::getMouseCursor(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    return QCursor(Qt::PointingHandCursor);
}

#include "trackpanelmarkerfunction.moc"
