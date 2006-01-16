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
#include "ktimeline.h"
#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "kmoveclipscommand.h"
#include "kselectclipcommand.h"

TrackPanelSpacerFunction::TrackPanelSpacerFunction(Gui::KdenliveApp * app, Gui::KTimeLine * timeline, KdenliveDoc * doc):
m_app(app),
m_timeline(timeline), m_doc(doc), m_masterClip(0), m_moveClipsCommand(0)
{
}


TrackPanelSpacerFunction::~TrackPanelSpacerFunction()
{
}

bool TrackPanelSpacerFunction::mouseApplies(Gui::KTrackPanel * panel,
    QMouseEvent * event) const
{
    return true;
}

QCursor TrackPanelSpacerFunction::getMouseCursor(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    return QCursor(Qt::SizeHorCursor);
}

bool TrackPanelSpacerFunction::mousePressed(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    GenTime mouseTime(m_timeline->mapLocalToValue(event->x()),
	m_doc->framesPerSecond());
    GenTime roundedMouseTime = m_timeline->timeUnderMouse(event->x());
    m_clipUnderMouse = 0;

    if (event->state() & ShiftButton) {
	m_app->
	    addCommand(Command::KSelectClipCommand::selectLaterClips(m_doc,
		mouseTime, false), true);
    } else {
	m_app->
	    addCommand(Command::KSelectClipCommand::selectLaterClips(m_doc,
		mouseTime, true), true);
    }

    if (m_doc->hasSelectedClips()) {
	m_masterClip = m_doc->selectedClip();
	m_moveClipsCommand =
	    new Command::KMoveClipsCommand(m_doc, m_masterClip);
	m_clipOffset = mouseTime - m_masterClip->trackStart();

	m_snapToGrid.clearSnapList();
	m_snapToGrid.setSnapToFrame(m_timeline->snapToFrame());
	if (m_timeline->snapToSeekTime())
	    m_snapToGrid.addToSnapList(m_timeline->seekPosition());
	m_snapToGrid.setSnapTolerance(GenTime(m_timeline->
		mapLocalToValue(Gui::KTimeLine::snapTolerance) -
		m_timeline->mapLocalToValue(0), m_doc->framesPerSecond()));

	m_snapToGrid.addToSnapList(m_doc->getSnapTimes(m_timeline->
		snapToBorders(), m_timeline->snapToMarkers(), true,
		false));

	QValueVector < GenTime > cursor =
	    m_doc->getSnapTimes(m_timeline->snapToBorders(),
	    m_timeline->snapToMarkers(), false, true);
	m_snapToGrid.setCursorTimes(cursor);
    }

    return true;
}

bool TrackPanelSpacerFunction::mouseReleased(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    if (m_moveClipsCommand) {
	m_moveClipsCommand->setEndLocation(m_masterClip);
	m_app->addCommand(m_moveClipsCommand, false);
	m_moveClipsCommand = 0;
    }

    return true;
}

bool TrackPanelSpacerFunction::mouseMoved(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    GenTime mouseTime =
	m_timeline->timeUnderMouse(event->x()) - m_clipOffset;
    mouseTime = m_snapToGrid.getSnappedTime(mouseTime);
    mouseTime = mouseTime + m_clipOffset;

    if (m_moveClipsCommand) {
	GenTime startOffset =
	    mouseTime - m_clipOffset - m_masterClip->trackStart();
	m_doc->moveSelectedClips(startOffset, 0);
	m_timeline->drawTrackViewBackBuffer();
    }

    return true;
}
