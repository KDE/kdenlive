/***************************************************************************
                          trackpanelcliprollfunction  -  description
                             -------------------
    begin                : Wed Aug 25 2004
    copyright            : (C) 2004 by Jason Wood
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
#include "trackpanelcliprollfunction.h"

#include "kdebug.h"

#include <qnamespace.h>

#include "clipdrag.h"
#include "doctrackbase.h"
#include "kdenlive.h"
#include "ktimeline.h"
#include "krollcommand.h"
#include "kselectclipcommand.h"
#include "ktrackview.h"

#include <cmath>

// static
const uint TrackPanelClipRollFunction::s_resizeTolerance = 5;

TrackPanelClipRollFunction::TrackPanelClipRollFunction(KdenliveApp *app,
								KTimeLine *timeline,
								KdenliveDoc *document) :
								m_app(app),
								m_timeline(timeline),
								m_document(document),
								m_clipUnderMouse(0),
								m_resizeState(None),
								m_rollCommand(0),
								m_snapToGrid()
{
}

TrackPanelClipRollFunction::~TrackPanelClipRollFunction()
{
}
//make sure there are two concurrent tracks before applying mouse
bool TrackPanelClipRollFunction::mouseApplies(KTrackPanel *panel, QMouseEvent *event) const
{
	bool result = false;

	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
			DocClipRef *clip = track->getClipAt(mouseTime);
			if(clip) {
				if( fabs(m_timeline->mapValueToLocal(clip->trackStart().frames(m_document->framesPerSecond())) - event->x()) < s_resizeTolerance) {
					GenTime beforeTime(5.0, m_document->framesPerSecond());
					beforeTime = clip->trackStart() - beforeTime;
					DocClipRef *clipBefore = track->getClipAt(beforeTime);
					if(clipBefore){
						result = true;
					}
				}
				if( fabs(m_timeline->mapValueToLocal((clip->trackEnd()).frames(m_document->framesPerSecond())) - event->x()) < s_resizeTolerance) {
					GenTime afterTime(5.0, m_document->framesPerSecond());
					afterTime = clip->trackEnd() + afterTime;
					DocClipRef *clipAfter = track->getClipAt(afterTime);
					if(clipAfter){
						result = true;
					}
				}
			}
		}
	}

	return result;
}

QCursor TrackPanelClipRollFunction::getMouseCursor(KTrackPanel *panel, QMouseEvent *event)
{
	return QCursor(Qt::SplitHCursor);
}

bool TrackPanelClipRollFunction::mousePressed(KTrackPanel *panel, QMouseEvent *event)
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
				//select both clips simultaneously
				GenTime beforeTime(1.0, m_document->framesPerSecond());
				if(m_resizeState == Start){
					m_app->addCommand(
							Command::KSelectClipCommand::selectClipAt(
										m_document,
										*track,
										(m_clipUnderMouse->trackStart() + beforeTime)));
										
				
					m_app->addCommand(
							Command::KSelectClipCommand::selectClipAt(
										m_document,
										*track,
										(m_clipUnderMouse->trackStart() - beforeTime )));
					m_clipBeforeMouse = track->getClipAt( GenTime( m_clipUnderMouse->trackStart() - beforeTime ) );
				}
				if(m_resizeState == End){
					m_app->addCommand(
							Command::KSelectClipCommand::selectClipAt(
										m_document,
										*track,
										(m_clipUnderMouse->trackEnd() + beforeTime)));
					m_clipAfterMouse = track->getClipAt(GenTime( m_clipUnderMouse->trackEnd() + beforeTime ));
				
					m_app->addCommand(
							Command::KSelectClipCommand::selectClipAt(
										m_document,
										*track,
										(m_clipUnderMouse->trackEnd() - beforeTime )));
				}		
				m_snapToGrid.clearSnapList();
				if(m_timeline->snapToSeekTime()) m_snapToGrid.addToSnapList(m_timeline->seekPosition());
				m_snapToGrid.setSnapToFrame(m_timeline->snapToFrame());

				m_snapToGrid.addToSnapList(m_document->getSnapTimes(m_timeline->snapToBorders(),
															m_timeline->snapToMarkers(),
															true,
															false));

				m_snapToGrid.setSnapTolerance(GenTime(m_timeline->mapLocalToValue(KTimeLine::snapTolerance) - m_timeline->mapLocalToValue(0), m_document->framesPerSecond()));

				QValueVector<GenTime> cursor;

				if(m_resizeState == Start) {
					cursor.append(m_clipUnderMouse->trackStart());
				}
				else if (m_resizeState == End) {
					cursor.append(m_clipUnderMouse->trackEnd());
				}
				m_snapToGrid.setCursorTimes(cursor);
				m_rollCommand = new Command::KRollCommand(m_document, *m_clipUnderMouse);

				result = true;
			}
		}
	}

	return result;
}

bool TrackPanelClipRollFunction::mouseReleased(KTrackPanel *panel, QMouseEvent *event)
{
	bool result = false;

	m_rollCommand->setEndSize(*m_clipUnderMouse);
	if(m_clipBeforeMouse) {
		m_rollCommand->setEndSize(*m_clipBeforeMouse);
		//m_clipBeforeMouse->setTrackEnd( m_clipUnderMouse->trackStart() );
	}
	if(m_clipAfterMouse){
		m_rollCommand->setEndSize(*m_clipAfterMouse);
		//m_clipAfterMouse->setTrackStart( m_clipUnderMouse->trackEnd() );
	}
	m_app->addCommand(m_rollCommand, false);
	m_document->indirectlyModified();
	m_rollCommand = 0;

	result = true;
	return result;
}

bool TrackPanelClipRollFunction::mouseMoved(KTrackPanel *panel, QMouseEvent *event)
{
	bool result = false;

	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime = m_snapToGrid.getSnappedTime(m_timeline->timeUnderMouse(event->x()));
			
			if(m_clipUnderMouse) {
				result = true;
				
				if(m_resizeState == Start) {
					if(m_clipUnderMouse->trackEnd() - mouseTime < m_clipUnderMouse->duration() && mouseTime - m_clipBeforeMouse->cropStartTime() < m_clipBeforeMouse->duration()){						track->resizeClipTrackStart(m_clipUnderMouse, mouseTime);
						emit signalClipCropStartChanged(m_clipUnderMouse);
							
						track->resizeClipTrackEnd(m_clipBeforeMouse, mouseTime);
						emit signalClipCropEndChanged(m_clipBeforeMouse);
					}
				} else if(m_resizeState == End) {
					GenTime cropDuration = mouseTime - m_clipUnderMouse->trackStart();
					if( cropDuration < m_clipUnderMouse->duration() - m_clipUnderMouse->cropStartTime() && m_clipAfterMouse->trackEnd() - mouseTime < m_clipAfterMouse->duration()) {
						track->resizeClipTrackEnd(m_clipUnderMouse, mouseTime);
						emit signalClipCropEndChanged(m_clipUnderMouse);
					
						track->resizeClipTrackStart(m_clipAfterMouse, mouseTime);
						emit signalClipCropStartChanged(m_clipAfterMouse);
					}
				} else {
					kdError() << "Unknown resize state reached in KMMTimeLineTrackView::mouseMoveEvent()" << endl;
					kdError() << "(this message should never be seen!)" << endl;
				}
			}
		}
	}

	return result;
}
