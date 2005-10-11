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
#include "trackpanelrazorfunction.h"

#include "docclipbase.h"
#include "documentmacrocommands.h"
#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"

TrackPanelRazorFunction::TrackPanelRazorFunction(Gui::KdenliveApp *app,
							Gui::KTimeLine *timeline,
							KdenliveDoc *document) :
								m_app(app),
								m_timeline(timeline),
								m_document(document),
								m_clipUnderMouse(0)
{
}


TrackPanelRazorFunction::~TrackPanelRazorFunction()
{
}

bool TrackPanelRazorFunction::mouseApplies(Gui::KTrackPanel *panel, QMouseEvent *event) const
{
	DocClipRef *clip = 0;
	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
			clip = track->getClipAt(mouseTime);
		}
	}
	return clip;
}

QCursor TrackPanelRazorFunction::getMouseCursor(Gui::KTrackPanel *panel, QMouseEvent *event)
{
	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
			DocClipRef *clip = track->getClipAt(mouseTime);
			if(clip) {
				emit lookingAtClip(clip, mouseTime - clip->trackStart() + clip->cropStartTime());
			}
		}
	}
	return QCursor(Qt::SplitVCursor);
}

bool TrackPanelRazorFunction::mousePressed(Gui::KTrackPanel *panel, QMouseEvent *event)
{
	if(panel->hasDocumentTrackIndex()) {
		DocTrackBase *track = m_document->track(panel->documentTrackIndex());
		if(track) {
			GenTime mouseTime(m_timeline->mapLocalToValue(event->x()), m_document->framesPerSecond());
			GenTime roundedMouseTime = m_timeline->timeUnderMouse(event->x());

			m_clipUnderMouse = track->getClipAt(mouseTime);
			if(m_clipUnderMouse) {
				if (event->state() & ShiftButton) {
					m_app->addCommand(Command::DocumentMacroCommands::razorAllClipsAt(m_document, roundedMouseTime), true);
				} else {
					m_app->addCommand(Command::DocumentMacroCommands::razorClipAt(m_document, *track, roundedMouseTime), true);
				}
				return true;
			}
		}
	}
	return true;
}

bool TrackPanelRazorFunction::mouseReleased(Gui::KTrackPanel *panel, QMouseEvent *event)
{
 	GenTime mouseTime = m_timeline->timeUnderMouse(event->x());
	m_clipUnderMouse = 0;
	return true;
}

bool TrackPanelRazorFunction::mouseMoved(Gui::KTrackPanel *panel, QMouseEvent *event)
{
	return true;
}
