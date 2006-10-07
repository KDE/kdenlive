/***************************************************************************
                          trackpanelselectnonefunction  -  description
                             -------------------
    begin                : Mon Dec 29 2003
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
#include "trackpanelselectnonefunction.h"

#include "kdenlivedoc.h"
#include "kselectclipcommand.h"

TrackPanelSelectNoneFunction::TrackPanelSelectNoneFunction(Gui::KdenliveApp * app, Gui::KTimeLine * timeline, KdenliveDoc * doc):
TrackPanelFunction(), m_app(app), m_timeline(timeline), m_doc(doc), m_fps(doc->framesPerSecond())
{
}


TrackPanelSelectNoneFunction::~TrackPanelSelectNoneFunction()
{
}

// virtual
bool TrackPanelSelectNoneFunction::mouseApplies(Gui::KTrackPanel * panel,
    QMouseEvent * event) const
{
    return true;
}

// virtual
QCursor TrackPanelSelectNoneFunction::getMouseCursor(Gui::KTrackPanel *
    panel, QMouseEvent * event)
{
    return QCursor(Qt::ArrowCursor);
}

// virtual
bool TrackPanelSelectNoneFunction::mousePressed(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    m_app->slotSeekTo( GenTime(m_timeline->mapLocalToValue(event->x()), m_fps));
    return true;
}

bool TrackPanelSelectNoneFunction::mouseDoubleClicked(Gui::KTrackPanel * panel, QMouseEvent * event)
{
    return true;
}

// virtual
bool TrackPanelSelectNoneFunction::mouseReleased(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    m_app->addCommand(Command::KSelectClipCommand::selectNone(m_doc),
	true);
    return true;
}

// virtual
bool TrackPanelSelectNoneFunction::mouseMoved(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    m_app->slotSeekTo( GenTime(m_timeline->mapLocalToValue(event->x()), m_fps));
    m_timeline->checkScrolling(event->pos());
    return true;
}
