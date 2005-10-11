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
#include "trackpanelclipresizefunction.h"

#include "kdebug.h"

#include "doctrackbase.h"
#include "kdenlive.h"
#include "ktimeline.h"
#include "kresizecommand.h"
#include "kselectclipcommand.h"

#include <cmath>

// static
const uint TrackPanelClipResizeFunction::s_resizeTolerance = 5;

TrackPanelClipResizeFunction::TrackPanelClipResizeFunction(Gui::KdenliveApp *app,
								Gui::KTimeLine *timeline,
								KdenliveDoc *document) :
								m_app(app),
								m_timeline(timeline),
								m_document(document),
								m_clipUnderMouse(0),
								m_resizeState(None),
								m_resizeCommand(0),
								m_snapToGrid()
{
}

TrackPanelClipResizeFunction::~TrackPanelClipResizeFunction()
{
}


bool TrackPanelClipResizeFunction::mouseApplies(Gui::KTrackPanel *panel, QMouseEvent *event) const
{
	bool result = false;

	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
			DocClipRef *clip = track->getClipAt(mouseTime);
			if(clip) {
				if( fabs(m_timeline->mapValueToLocal(clip->trackStart().frames(m_document->framesPerSecond())) - event->x()) < s_resizeTolerance) {
					result = true;
				}
				if( fabs(m_timeline->mapValueToLocal((clip->trackEnd()).frames(m_document->framesPerSecond())) - event->x()) < s_resizeTolerance) {
					result = true;
				}
			}
		}
	}

	return result;
}

QCursor TrackPanelClipResizeFunction::getMouseCursor(Gui::KTrackPanel *panel, QMouseEvent *event)
{
	return QCursor(Qt::SizeHorCursor);
}

bool TrackPanelClipResizeFunction::mousePressed(Gui::KTrackPanel *panel, QMouseEvent *event)
{
	bool result = false;

	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
			m_clipUnderMouse = track->getClipAt(mouseTime);
			if(m_clipUnderMouse) {
				if( fabs(m_timeline->mapValueToLocal(m_clipUnderMouse->trackStart().frames(m_document->framesPerSecond())) - event->x()) < s_resizeTolerance) {
					m_resizeState = Start;
				}
				if( fabs(m_timeline->mapValueToLocal((m_clipUnderMouse->trackEnd()).frames(m_document->framesPerSecond())) - event->x()) < s_resizeTolerance) {
					m_resizeState = End;
				}

				m_app->addCommand(Command::KSelectClipCommand::selectNone(m_document), true);

				m_app->addCommand(
							Command::KSelectClipCommand::selectClipAt(
										m_document,
										*track,
										(m_clipUnderMouse->trackStart() + m_clipUnderMouse->trackEnd())/2.0));

				m_snapToGrid.clearSnapList();
				if(m_timeline->snapToSeekTime()) m_snapToGrid.addToSnapList(m_timeline->seekPosition());
				m_snapToGrid.setSnapToFrame(m_timeline->snapToFrame());

				m_snapToGrid.addToSnapList(m_document->getSnapTimes(m_timeline->snapToBorders(),
															m_timeline->snapToMarkers(),
															true,
															false));

				m_snapToGrid.setSnapTolerance(GenTime(m_timeline->mapLocalToValue(Gui::KTimeLine::snapTolerance) - m_timeline->mapLocalToValue(0), m_document->framesPerSecond()));

				QValueVector<GenTime> cursor;

				if(m_resizeState == Start) {
					cursor.append(m_clipUnderMouse->trackStart());
				}
				else if (m_resizeState == End) {
					cursor.append(m_clipUnderMouse->trackEnd());
				}
				m_snapToGrid.setCursorTimes(cursor);
				m_resizeCommand = new Command::KResizeCommand(m_document, *m_clipUnderMouse);

				result = true;
			}
		}
	}

	return result;
}

bool TrackPanelClipResizeFunction::mouseReleased(Gui::KTrackPanel *panel, QMouseEvent *event)
{
	bool result = false;

	m_resizeCommand->setEndSize(*m_clipUnderMouse);
	m_app->addCommand(m_resizeCommand, false);
	m_document->indirectlyModified();
	m_resizeCommand = 0;

	result = true;
	return result;
}

bool TrackPanelClipResizeFunction::mouseMoved(Gui::KTrackPanel *panel, QMouseEvent *event)
{
	bool result = false;

	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime = m_snapToGrid.getSnappedTime(m_timeline->timeUnderMouse(event->x()));

			if(m_clipUnderMouse) {
				result = true;
				if(m_resizeState == Start) {
					track->resizeClipTrackStart(m_clipUnderMouse, mouseTime);
					emit signalClipCropStartChanged(m_clipUnderMouse);
				} else if(m_resizeState == End) {
					track->resizeClipTrackEnd(m_clipUnderMouse, mouseTime);
					emit signalClipCropEndChanged(m_clipUnderMouse);
				} else {
					kdError() << "Unknown resize state reached in KMMTimeLineTrackView::mouseMoveEvent()" << endl;
					kdError() << "(this message should never be seen!)" << endl;
				}
			}
		}
	}

	return result;
}
