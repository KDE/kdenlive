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
#include "kmmtimeline.h"
#include "kresizecommand.h"

#include <cmath>

// static
const uint TrackPanelClipResizeFunction::s_resizeTolerance = 5;

TrackPanelClipResizeFunction::TrackPanelClipResizeFunction(KMMTimeLine *timeline, 
								KdenliveDoc *document,
								DocTrackBase *docTrack) :
								m_timeline(timeline),
								m_document(document),
								m_docTrack(docTrack),
								m_clipUnderMouse(0),
								m_resizeState(None),
								m_resizeCommand(0),
								m_snapToGrid(document)
{
}

TrackPanelClipResizeFunction::~TrackPanelClipResizeFunction()
{
}


bool TrackPanelClipResizeFunction::mouseApplies(QMouseEvent *event) const
{
	bool result = false;

	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	DocClipRef *clip = m_docTrack->getClipAt(mouseTime);
	if(clip) {
		if( fabs(m_timeline->mapValueToLocal(clip->trackStart().frames(m_document->framesPerSecond())) - event->x()) < s_resizeTolerance) {
			result = true;
		}
		if( fabs(m_timeline->mapValueToLocal((clip->trackEnd()).frames(m_document->framesPerSecond())) - event->x()) < s_resizeTolerance) {
			result = true;
		}
	}

	return result;
}

QCursor TrackPanelClipResizeFunction::getMouseCursor(QMouseEvent *event)
{
	return QCursor(Qt::SizeHorCursor);
}

bool TrackPanelClipResizeFunction::mousePressed(QMouseEvent *event)
{
	bool result = false;
	
	GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
	m_clipUnderMouse = m_docTrack->getClipAt(mouseTime);
	if(m_clipUnderMouse) {
		if( fabs(m_timeline->mapValueToLocal(m_clipUnderMouse->trackStart().frames(m_document->framesPerSecond())) - event->x()) < s_resizeTolerance) {
			m_resizeState = Start;
		}
		if( fabs(m_timeline->mapValueToLocal((m_clipUnderMouse->trackEnd()).frames(m_document->framesPerSecond())) - event->x()) < s_resizeTolerance) {
			m_resizeState = End;
		}

	  	m_timeline->addCommand(m_timeline->selectNone(), true);
		m_timeline->selectClipAt(*m_docTrack, (m_clipUnderMouse->trackStart() + m_clipUnderMouse->trackEnd())/2.0);
		m_snapToGrid.setSnapToClipStart(m_timeline->snapToBorders());
		m_snapToGrid.setSnapToClipEnd(m_timeline->snapToBorders());
		m_snapToGrid.setSnapToFrame(m_timeline->snapToFrame());
		m_snapToGrid.setSnapToSeekTime(m_timeline->snapToSeekTime());
		m_snapToGrid.setSnapTolerance(GenTime(m_timeline->mapLocalToValue(KMMTimeLine::snapTolerance) - m_timeline->mapLocalToValue(0), m_document->framesPerSecond()));
		m_snapToGrid.setIncludeSelectedClips(false);
		m_snapToGrid.clearSeekTimes();
		m_snapToGrid.addSeekTime(m_timeline->seekPosition());
		QValueList<GenTime> cursor;

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

	return result;
}

bool TrackPanelClipResizeFunction::mouseReleased(QMouseEvent *event)
{
	bool result = false;
	
	m_resizeCommand->setEndSize(*m_clipUnderMouse);
	m_timeline->addCommand(m_resizeCommand, false);
	m_document->indirectlyModified();
	m_resizeCommand = 0;

	result = true;
	return result;
}

bool TrackPanelClipResizeFunction::mouseMoved(QMouseEvent *event)
{
	GenTime mouseTime = m_snapToGrid.getSnappedTime(m_timeline->timeUnderMouse(event->x()));
	
	if(m_clipUnderMouse) {
		if(m_resizeState == Start) {
			m_docTrack->resizeClipTrackStart(m_clipUnderMouse, mouseTime);
			emit signalClipCropStartChanged(m_clipUnderMouse);
		} else if(m_resizeState == End) {
			m_docTrack->resizeClipTrackEnd(m_clipUnderMouse, mouseTime);
			emit signalClipCropEndChanged(m_clipUnderMouse);
		} else {
			kdError() << "Unknown resize state reached in KMMTimeLineTrackView::mouseMoveEvent()" << endl;
			kdError() << "(this message should never be seen!)" << endl;
		}
	}
}
