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
#include "ktrackview.h"
#include "kselectclipcommand.h"

TrackPanelSelectNoneFunction::TrackPanelSelectNoneFunction(Gui::KdenliveApp * app, Gui::KTimeLine * timeline, KdenliveDoc * doc):
TrackPanelFunction(), m_app(app), m_timeline(timeline), m_doc(doc), m_fps(doc->framesPerSecond()), m_multiselect(false)
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
    if (event->state() & Qt::ShiftButton) {
	m_multiselect = true;
	m_multiselectStart = QPoint(m_timeline->mapLocalToValue(event->x()), event->y());
    }
    else {
	m_app->activateWorkspaceMonitor();
	m_app->slotSeekTo( GenTime(m_timeline->mapLocalToValue(event->x()), m_fps));
    }
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
    if (m_multiselect) {
	QPoint end = QPoint(m_timeline->mapLocalToValue(event->x()), event->y());
	m_timeline->finishMultiSelection(m_multiselectStart, end);
	QRect rect(m_multiselectStart.x(), m_multiselectStart.y(), end.x() - m_multiselectStart.x(), end.y() - m_multiselectStart.y());
	rect = rect.normalize();
	KMacroCommand *macroCommand = new KMacroCommand(i18n("Select Clip"));
	macroCommand->addCommand(Command::KSelectClipCommand::selectNone(m_doc));
	macroCommand->addCommand(Command::KSelectClipCommand::selectRectangleClips(m_doc, m_timeline->trackView()->panelAt( rect.top())->documentTrackIndex(), m_timeline->trackView()->panelAt( rect.bottom())->documentTrackIndex(), GenTime(rect.left(),m_fps) , GenTime(rect.right(), m_fps), true));
	m_app->addCommand( macroCommand, true);
    }
    else m_app->addCommand(Command::KSelectClipCommand::selectNone(m_doc),
	true);
    m_multiselect = false;
    m_timeline->stopScrollTimer();
    return true;
}

// virtual
bool TrackPanelSelectNoneFunction::mouseMoved(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    if (m_multiselect)
    {
	QPoint end = QPoint(m_timeline->mapLocalToValue(event->x()), event->y());
	m_timeline->drawSelection(m_multiselectStart, end);
    }
    else {
        m_app->slotSeekTo( GenTime(m_timeline->mapLocalToValue(event->x()), m_fps));
    }
    m_timeline->checkScrolling(event->pos());
    return true;
}
