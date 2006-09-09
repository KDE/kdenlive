/***************************************************************************
                          kaddmarkercommand  -  description
                             -------------------
    begin                : Sat Nov 29 2003
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
#include "kaddmarkercommand.h"

#include "kdebug.h"
#include "klocale.h"
#include <kinputdialog.h>

#include "docclipref.h"
#include "docclipproject.h"
#include "doctrackbase.h"
#include "kdenlivedoc.h"

namespace Command {

    KAddMarkerCommand::KAddMarkerCommand(KdenliveDoc & document,
	DocClipRef * clip,
	const GenTime & clipTime, QString comment,
	bool create):KCommand(),
	m_document(document),
	m_create(create),
	m_clipTime(clipTime), m_comment(comment), 
	m_trackTime(clip->trackMiddleTime()), m_track(clip->trackNum()) {
    } KAddMarkerCommand::~KAddMarkerCommand() {
    }

/** Returns the name of this command */
    QString KAddMarkerCommand::name() const {
	if (m_create) {
	    return i18n("Add Snap Marker");
	} else {
	    return i18n("Delete Snap Marker");
	}
    }

/** Execute the command */
    void KAddMarkerCommand::execute() {
	if (m_create) {
	    addMarker();
	} else {
	    deleteMarker();
	}
    }

/** Unexecute the command */
    void KAddMarkerCommand::unexecute() {
	if (m_create) {
	    deleteMarker();
	} else {
	    addMarker();
	}
    }

    void KAddMarkerCommand::addMarker() {
	DocTrackBase *track = m_document.projectClip().track(m_track);

	if (track) {
	    DocClipRef *clip = track->getClipAt(m_trackTime);
	    if (clip) {
		clip->addSnapMarker(m_clipTime, m_comment);
	    } else {
		kdError() <<
		    "Trying to add marker; no clip exists at this point on the track!"
		    << endl;
	    }

	} else {
	    kdError() <<
		"KAddMarkerCommand : trying to add marker, specified track "
		<< m_track << " does not exist in the project!" << endl;
	}
    }

    void KAddMarkerCommand::deleteMarker() {
	DocTrackBase *track = m_document.projectClip().track(m_track);

	if (track) {
	    DocClipRef *clip = track->getClipAt(m_trackTime);
	    if (clip) {
		clip->deleteSnapMarker(m_clipTime);
	    } else {
		kdError() <<
		    "Trying to delete marker; no clip exists at this point on the track!"
		    << endl;
	    }

	} else {
	    kdError() <<
		"KAddMarkerCommand : trying to delete marker, specified track "
		<< m_track << " does not exist in the project!" << endl;
	}
    }

}
