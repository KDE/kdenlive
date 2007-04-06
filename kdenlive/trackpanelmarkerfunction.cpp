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
#include "keditmarkercommand.h"
#include "kdenlive.h"
#include "kdenlivedoc.h"
#include "ktimeline.h"
#include "kinputdialog.h"

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
		GenTime mouseTime((int)(m_timeline->mapLocalToValue(event->x())), m_document->framesPerSecond());
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

bool TrackPanelMarkerFunction::mouseDoubleClicked(Gui::KTrackPanel * panel, QMouseEvent * event)
{
    return false;
}

bool TrackPanelMarkerFunction::mouseReleased(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    DocClipRef *clipUnderMouse = 0;

    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track) {
		GenTime mouseTime((int)(m_timeline->mapLocalToValue(event->x())),
		m_document->framesPerSecond());
	    clipUnderMouse = track->getClipAt(mouseTime);
	    //kdDebug()<<"*** get ready for marker on track: "<<panel->documentTrackIndex()<<endl;
	    if (clipUnderMouse) {
		QValueVector < CommentedTime > markers = clipUnderMouse->commentedSnapMarkers();
		QValueVector < CommentedTime >::iterator itt = markers.begin();
		while (itt != markers.end()) {
		    int x = (int)(m_timeline->mapValueToLocal(((*itt).time() + clipUnderMouse->trackStart() - clipUnderMouse->cropStartTime()).frames(m_document->framesPerSecond())));
		    if (fabs(x - event->x()) < 10) {
			if (event->state() & Qt::ControlButton) {
			    // Pressing CTRL removes current marker
			    Command::KAddMarkerCommand * command = new Command::KAddMarkerCommand(*m_document, clipUnderMouse->referencedClip()->getId(), (*itt).time(), (*itt).comment(), false);
		    	    m_app->addCommand(command);
			    return true;
			}
			bool ok;
		        QString comment = KInputDialog::getText(i18n("Edit Marker"), i18n("Marker comment: "), (*itt).comment(), &ok);
			if (ok) {
			    Command::KEditMarkerCommand * command = new Command::KEditMarkerCommand(*m_document, clipUnderMouse, (*itt).time(), comment, true);
		     	    m_app->addCommand(command);
			}
			return true;
		    }
		    ++itt;
		}
		//kdDebug()<<"*** get ready for marker on clip: "<<clipUnderMouse->name()<<endl;
		bool ok;
		QString comment = KInputDialog::getText(i18n("Add Marker"), i18n("Marker comment: "), i18n("Marker"), &ok);
		if (ok) {
		    Command::KAddMarkerCommand * command = new Command::KAddMarkerCommand(*m_document, clipUnderMouse->referencedClip()->getId(), mouseTime - clipUnderMouse->trackStart() + clipUnderMouse->cropStartTime(), comment, true);
		    m_app->addCommand(command);
		}
	    }
	}
    }

    return true;
}

QCursor TrackPanelMarkerFunction::getMouseCursor(Gui::KTrackPanel * panel,
    QMouseEvent * event)
{
    if (panel->hasDocumentTrackIndex()) {
	DocTrackBase *track =
	    m_document->track(panel->documentTrackIndex());
	if (track && track->clipType() == "Video") {
		GenTime mouseTime((int)(m_timeline->mapLocalToValue(event->x())), m_document->framesPerSecond());
	        DocClipRef *clipUnderMouse = track->getClipAt(mouseTime);
	    if (clipUnderMouse) {
		emit lookingAtClip(clipUnderMouse,
		    mouseTime - clipUnderMouse->trackStart() +
		    clipUnderMouse->cropStartTime());
	    }
	}
    }
    return QCursor(Qt::PointingHandCursor);
}

