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
#include <qstring.h>

#include "clipdrag.h"
#include "doctrackbase.h"
#include "kdenlive.h"
#include "ktimeline.h"
#include "krollcommand.h"
#include "kselectclipcommand.h"
#include "ktrackview.h"

#include "kmmtimeline.h"

#include <cmath>

// static
const uint TrackPanelClipRollFunction::s_resizeTolerance = 5;

TrackPanelClipRollFunction::TrackPanelClipRollFunction(Gui::KdenliveApp * app, Gui::KTimeLine * timeline, KdenliveDoc * document):
m_app(app),
m_timeline(timeline),
m_document(document),
m_clipUnderMouse(0), m_resizeState(None), m_rollCommand(0), m_snapToGrid()
{

}

TrackPanelClipRollFunction::~TrackPanelClipRollFunction()
{
}

//make sure there are two concurrent tracks before applying mouse
bool TrackPanelClipRollFunction::mouseApplies(Gui::KTrackPanel * panel,
    QMouseEvent * event) const
{
    bool result = false;
    //get current timescale and set minimum crop time
    double minDrag = getMinimumDrag();

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime(m_timeline->mapLocalToValue(event->x()),
		m_document->framesPerSecond());
	    DocClipRef *clip = track->getClipAt(mouseTime);
	    if (clip) {
		if (fabs(m_timeline->mapValueToLocal(clip->trackStart().
			    frames(m_document->framesPerSecond())) -
			event->x()) < s_resizeTolerance) {
		    GenTime beforeTime(minDrag,
			m_document->framesPerSecond());
		    beforeTime = clip->trackStart() - beforeTime;
		    DocClipRef *clipBefore = track->getClipAt(beforeTime);
		    if (clipBefore) {
			result = true;
		    }
		}
		if (fabs(m_timeline->mapValueToLocal((clip->trackEnd()).
			    frames(m_document->framesPerSecond())) -
			event->x()) < s_resizeTolerance) {
		    GenTime afterTime(minDrag,
			m_document->framesPerSecond());
		    afterTime = clip->trackEnd() + afterTime;
		    DocClipRef *clipAfter = track->getClipAt(afterTime);
		    if (clipAfter) {
			result = true;
		    }
		}
	    }
	}
    }

    return result;
}

QCursor TrackPanelClipRollFunction::getMouseCursor(Gui::KTrackPanel *
    panel, QMouseEvent * event)
{
    return QCursor(Qt::SplitHCursor);
}

bool TrackPanelClipRollFunction::mousePressed(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());

	if (track) {
	    GenTime mouseTime(m_timeline->mapLocalToValue(event->x()),
		m_document->framesPerSecond());
	    m_clipUnderMouse = track->getClipAt(mouseTime);
	    if (m_clipUnderMouse) {
		if (fabs(m_timeline->mapValueToLocal(m_clipUnderMouse->
			    trackStart().frames(m_document->
				framesPerSecond())) - event->x()) <
		    s_resizeTolerance) {
		    m_resizeState = Start;
		}
		if (fabs(m_timeline->mapValueToLocal((m_clipUnderMouse->
				trackEnd()).frames(m_document->
				framesPerSecond())) - event->x()) <
		    s_resizeTolerance) {
		    m_resizeState = End;
		}

		m_app->
		    addCommand(Command::KSelectClipCommand::
		    selectNone(m_document), true);
		//select both clips simultaneously
		GenTime beforeTime(1.0, m_document->framesPerSecond());
		if (m_resizeState == Start) {
		    m_app->
			addCommand(Command::KSelectClipCommand::
			selectClipAt(m_document, *track,
			    (m_clipUnderMouse->trackStart() +
				beforeTime)));


		    m_app->
			addCommand(Command::KSelectClipCommand::
			selectClipAt(m_document, *track,
			    (m_clipUnderMouse->trackStart() -
				beforeTime)));
		    m_clipBeforeMouse =
			track->getClipAt(GenTime(m_clipUnderMouse->
			    trackStart() - beforeTime));
		}
		if (m_resizeState == End) {
		    m_app->
			addCommand(Command::KSelectClipCommand::
			selectClipAt(m_document, *track,
			    (m_clipUnderMouse->trackEnd() + beforeTime)));
		    m_clipAfterMouse =
			track->getClipAt(GenTime(m_clipUnderMouse->
			    trackEnd() + beforeTime));

		    m_app->
			addCommand(Command::KSelectClipCommand::
			selectClipAt(m_document, *track,
			    (m_clipUnderMouse->trackEnd() - beforeTime)));
		}
		m_snapToGrid.clearSnapList();
		if (m_timeline->snapToSeekTime())
		    m_snapToGrid.addToSnapList(m_timeline->seekPosition());
		m_snapToGrid.setSnapToFrame(m_timeline->snapToFrame());

		m_snapToGrid.addToSnapList(m_document->
		    getSnapTimes(m_timeline->snapToBorders(),
			m_timeline->snapToMarkers(), true, false));

		m_snapToGrid.setSnapTolerance(GenTime(m_timeline->
			mapLocalToValue(Gui::KTimeLine::snapTolerance) -
			m_timeline->mapLocalToValue(0),
			m_document->framesPerSecond()));

		QValueVector < GenTime > cursor;

		if (m_resizeState == Start) {
		    cursor.append(m_clipUnderMouse->trackStart());
		} else if (m_resizeState == End) {
		    cursor.append(m_clipUnderMouse->trackEnd());
		}
		m_snapToGrid.setCursorTimes(cursor);
		if (m_resizeState == Start) {
		    m_rollCommand =
			new Command::KRollCommand(m_document,
			*m_clipUnderMouse, *m_clipBeforeMouse);
		} else if (m_resizeState == End) {
		    m_rollCommand =
			new Command::KRollCommand(m_document,
			*m_clipUnderMouse, *m_clipAfterMouse);
		}

		result = true;
	    }
	}
    }

    return result;
}

bool TrackPanelClipRollFunction::mouseDoubleClicked(Gui::KTrackPanel * panel, QMouseEvent * event)
{
    return true;
}

bool TrackPanelClipRollFunction::mouseReleased(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;

    if (m_clipBeforeMouse) {
	m_rollCommand->setEndSize(*m_clipUnderMouse, *m_clipBeforeMouse);
    }
    if (m_clipAfterMouse) {
	m_rollCommand->setEndSize(*m_clipUnderMouse, *m_clipAfterMouse);
    }
    m_app->addCommand(m_rollCommand, false);
    m_document->indirectlyModified();
    m_rollCommand = 0;

    result = true;
    return result;
}

bool TrackPanelClipRollFunction::mouseMoved(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    bool result = false;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
	    GenTime mouseTime =
		m_snapToGrid.getSnappedTime(m_timeline->
		timeUnderMouse(event->x()));

	    if (m_clipUnderMouse) {
		result = true;

		if (m_resizeState == Start) {
		    if (mouseTime < m_clipUnderMouse->trackStart()) {
			//dragging left, mouse on beginning of second track
			if (m_clipBeforeMouse->cropDuration() >
			    GenTime(getMinimumDrag())
			    && m_clipUnderMouse->cropStartTime() >
			    GenTime(getMinimumDrag())) {
			    track->resizeClipTrackEnd(m_clipBeforeMouse,
				mouseTime);
			    emit signalClipCropEndChanged
				(m_clipBeforeMouse);

			    track->resizeClipTrackStart(m_clipUnderMouse,
				m_clipBeforeMouse->trackEnd());
			    emit signalClipCropStartChanged
				(m_clipUnderMouse);
			}
		    } else {
			//dragging right, mouse on beginning of second track
			if (m_clipBeforeMouse->trackEnd() -
			    m_clipBeforeMouse->cropStartTime() <=
			    m_clipBeforeMouse->duration() -
			    GenTime(getMinimumDrag())
			    && m_clipUnderMouse->cropDuration() >
			    GenTime(getMinimumDrag())) {
			    track->resizeClipTrackStart(m_clipUnderMouse,
				mouseTime);
			    emit signalClipCropStartChanged
				(m_clipUnderMouse);

			    track->resizeClipTrackEnd(m_clipBeforeMouse,
				m_clipUnderMouse->trackStart());
			    emit signalClipCropEndChanged
				(m_clipBeforeMouse);
			}
		    }
		} else if (m_resizeState == End) {
		    GenTime cropDuration =
			mouseTime - m_clipUnderMouse->trackStart();
		    if (mouseTime > m_clipUnderMouse->trackEnd()) {
			//dragging right, mouse on end of first track
			if (m_clipAfterMouse->cropDuration() >
			    GenTime(getMinimumDrag())
			    && m_clipUnderMouse->trackEnd() -
			    m_clipUnderMouse->cropStartTime() <=
			    m_clipUnderMouse->duration() -
			    GenTime(getMinimumDrag())) {
			    track->resizeClipTrackStart(m_clipAfterMouse,
				mouseTime);
			    emit signalClipCropStartChanged
				(m_clipAfterMouse);

			    track->resizeClipTrackEnd(m_clipUnderMouse,
				m_clipAfterMouse->trackStart());
			    emit signalClipCropEndChanged
				(m_clipUnderMouse);
			}
		    } else {
			//dragging left, mouse on end of first track
			if (m_clipAfterMouse->cropStartTime() >
			    GenTime(getMinimumDrag())
			    && m_clipUnderMouse->cropDuration() >
			    GenTime(getMinimumDrag())) {
			    track->resizeClipTrackEnd(m_clipUnderMouse,
				mouseTime);
			    emit signalClipCropEndChanged
				(m_clipUnderMouse);

			    track->resizeClipTrackStart(m_clipAfterMouse,
				m_clipUnderMouse->trackEnd());
			    emit signalClipCropStartChanged
				(m_clipAfterMouse);
			}
		    }
		} else {
		    kdError() <<
			"Unknown resize state reached in trackpanelrollfunction::mouseMoved()"
			<< endl;
		    kdError() << "(this message should never be seen!)" <<
			endl;
		}
	    }
	}
    }

    return result;
}

double TrackPanelClipRollFunction::getMinimumDrag() const
{
    QString currentScale(m_app->getTimeScaleSliderText());
    double minimumDrag = 1.0;
    if (currentScale == "1 Frame") {
	minimumDrag = 0.05;
    } else if (currentScale == "2 Frames") {
	minimumDrag = 0.05;
    } else if (currentScale == "5 Frames") {
	minimumDrag = 0.05;
    } else if (currentScale == "10 Frames") {
	minimumDrag = 0.1;
    } else if (currentScale == "1 Second") {
	minimumDrag = 0.5;
    } else if (currentScale == "2 Seconds") {
	minimumDrag = 0.5;
    } else if (currentScale == "5 Seconds") {
	minimumDrag = 1.0;
    } else if (currentScale == "10 Seconds") {
	minimumDrag = 2.0;
    } else if (currentScale == "20 Seconds") {
	minimumDrag = 5.0;
    } else if (currentScale == "30 Seconds") {
	minimumDrag = 5.0;
    } else if (currentScale == "1 Minute") {
	minimumDrag = 10.0;
    } else if (currentScale == "2 Minutes") {
	minimumDrag = 10.0;
    } else if (currentScale == "4 Minutes") {
	minimumDrag = 20.0;
    } else if (currentScale == "8 Minutes") {
	minimumDrag = 40.0;
    } else {
	minimumDrag = 1.0;
    }
    return minimumDrag;
}
