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

TrackPanelClipMoveFunction::TrackPanelClipMoveFunction(KMMTimeLine *timeline, DocTrackBase *docTrack) :
								m_timeline(timeline),
								m_docTrack(docTrack)
{
}


TrackPanelClipMoveFunction::~TrackPanelClipMoveFunction()
{
}

bool TrackPanelClipMoveFunction::mouseApplies(QMouseEvent *event) const
{
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_docTrack->document()->framesPerSecond());
	DocClipBase *clipUnderMouse = 0;
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

	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_docTrack->document()->framesPerSecond());
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
			GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_docTrack->document()->framesPerSecond());
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

	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_docTrack->document()->framesPerSecond());

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


/**
bool KMMTrackVideoPanel::mousePressed(QMouseEvent *event)
{
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_docTrack->document()->framesPerSecond());
	GenTime roundedMouseTime = m_timeline->timeUnderMouse(event->x());
	m_clipUnderMouse = 0;

	switch(m_timeline->editMode()) {
		case KdenliveApp::Move :
										m_clipUnderMouse = docTrack()->getClipAt(mouseTime);
										if(m_clipUnderMouse) {
											if(m_resizeState != None) {
												m_timeline->selectClipAt(*m_docTrack, (m_clipUnderMouse->trackStart() + m_clipUnderMouse->trackEnd())/2.0);
												m_snapToGrid.setSnapToClipStart(m_timeline->snapToBorders());
												m_snapToGrid.setSnapToClipEnd(m_timeline->snapToBorders());
												m_snapToGrid.setSnapToFrame(m_timeline->snapToFrame());
												m_snapToGrid.setSnapToSeekTime(m_timeline->snapToSeekTime());
												m_snapToGrid.setSnapTolerance(GenTime(m_timeline->mapLocalToValue(KMMTimeLine::snapTolerance) - m_timeline->mapLocalToValue(0), m_docTrack->document()->framesPerSecond()));
												m_snapToGrid.setIncludeSelectedClips(false);
												m_snapToGrid.clearSeekTimes();
												m_snapToGrid.addSeekTime(m_timeline->seekPosition());
												QValueList<GenTime> cursor;
												if(m_resizeState == Start) {
													cursor.append(m_clipUnderMouse->trackStart());
												} else if (m_resizeState == End) {
													cursor.append(m_clipUnderMouse->trackEnd());
												}
												m_snapToGrid.setCursorTimes(cursor);

												m_resizeCommand = new Command::KResizeCommand(m_docTrack->document(), m_clipUnderMouse);
											}
											return true;
										} else {
											m_timeline->addCommand(m_timeline->selectNone(), true);
										}
										break;


		case KdenliveApp::Razor :
										m_clipUnderMouse = docTrack()->getClipAt(mouseTime);
										if(m_clipUnderMouse) {
											if (event->state() & ShiftButton) {
												m_timeline->razorAllClipsAt(roundedMouseTime);
											} else {
												m_timeline->addCommand(m_timeline->razorClipAt(*m_docTrack, roundedMouseTime), true);
											}
											return true;
										}

										break;


		case KdenliveApp::Spacer :
										if (event->state() & ShiftButton) {
			                				m_timeline->addCommand(m_timeline->selectLaterClips(mouseTime, true), true);
			       						} else {
			                				m_timeline->addCommand(m_timeline->selectLaterClips(mouseTime, false), true);
			           					}
										return true;
										break;
		default :
										kdWarning() << "Error, mouse pressed over clips, unknown mode" << endl;
	}

	return false;
}

bool KMMTrackVideoPanel::mouseReleased(QMouseEvent *event)
{
 	GenTime mouseTime = m_timeline->timeUnderMouse(event->x());

	switch(m_timeline->editMode()) {
		case KdenliveApp::Move :
										if(m_resizeState != None) {
											m_resizeCommand->setEndSize(m_clipUnderMouse);
											m_timeline->addCommand(m_resizeCommand, false);
											m_resizeCommand = 0;
										} else {
											if(m_dragging) {
												m_dragging = false;
											} else {
												if(m_clipUnderMouse) {
													if (event->state() & ControlButton) {
													} else if(event->state() & ShiftButton) {
													} else {
														m_timeline->addCommand(m_timeline->selectNone(), true);
													  m_timeline->selectClipAt(*m_docTrack, mouseTime);
													}
												} else {
													m_timeline->addCommand(m_timeline->selectNone(), true);
												}
											}
										}
										break;
		case KdenliveApp::Razor :
										break;
		case KdenliveApp::Spacer :
										break;
		default :
										kdWarning() << "Error, mouse released over clips, unknown mode" << endl;
	}

	m_clipUnderMouse = 0;
	return true;
}

QCursor KMMTrackVideoPanel::getMouseCursor(QMouseEvent *event)
{
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_docTrack->document()->framesPerSecond());
	DocClipBase *clip;

	m_resizeState = None;

	switch(m_timeline->editMode()) {
		case KdenliveApp::Move :
								clip = docTrack()->getClipAt(mouseTime);
								if(clip) {
									if( fabs(m_timeline->mapValueToLocal(clip->trackStart().frames(m_docTrack->document()->framesPerSecond())) - event->x()) < resizeTolerance) {
										m_resizeState = Start;
										return QCursor(Qt::SizeHorCursor);
									}
									if( fabs(m_timeline->mapValueToLocal((clip->trackEnd()).frames(m_docTrack->document()->framesPerSecond())) - event->x()) < resizeTolerance) {
										m_resizeState = End;
										return QCursor(Qt::SizeHorCursor);
									}
								}
								return QCursor(Qt::ArrowCursor);
		case KdenliveApp::Razor :
               clip = docTrack()->getClipAt(mouseTime);
                if(clip) {
                  emit lookingAtClip(clip, mouseTime - clip->trackStart() + clip->cropStartTime());
                }
								return QCursor(Qt::SplitVCursor);
		case KdenliveApp::Spacer :
								return QCursor(Qt::SizeHorCursor);
		default:
								return QCursor(Qt::ArrowCursor);
	}
}

bool KMMTrackVideoPanel::mouseMoved(QMouseEvent *event)
{
	GenTime mouseTime = m_snapToGrid.getSnappedTime(m_timeline->timeUnderMouse(event->x()));

	switch(m_timeline->editMode()) {
		case KdenliveApp::Move :
								if(m_clipUnderMouse) {
									if(m_resizeState != None) {
										if(m_resizeState == Start) {
											m_docTrack->resizeClipTrackStart(m_clipUnderMouse, mouseTime);
											emit signalClipCropStartChanged(m_clipUnderMouse->cropStartTime());
										} else if(m_resizeState == End) {
											m_docTrack->resizeClipTrackEnd(m_clipUnderMouse, mouseTime);
											emit signalClipCropEndChanged(m_clipUnderMouse->cropStartTime() + m_clipUnderMouse->cropDuration());
										} else {
											kdError() << "Unknown resize state reached in KMMTimeLineTrackView::mouseMoveEvent()" << endl;
											kdError() << "(this message should never be seen!)" << endl;
										}
									} else {
									}
								}
								break;
		case KdenliveApp::Razor :
								break;
		case KdenliveApp::Spacer :
								break;
		default:
								kdWarning() << "Error, mouse moved over clips, unknown mode" << endl;
	}

	return true;
}
*/
