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
#include "keditmarkercommand.h"

#include "kdebug.h"
#include "klocale.h"
#include <kinputdialog.h>

#include "docclipref.h"
#include "docclipproject.h"
#include "doctrackbase.h"
#include "kdenlivedoc.h"

namespace Command {

    KEditMarkerCommand::KEditMarkerCommand(KdenliveDoc & document,
	DocClipRef * clip,
	const GenTime & clipTime, QString comment,
	bool create):KCommand(),
	m_document(document),
	m_create(create),
	m_clipTime(clipTime), m_comment(comment),
	m_trackTime(clip->trackMiddleTime()), m_track(clip->trackNum()), m_previousComment(QString::null) {
    } 

    KEditMarkerCommand::~KEditMarkerCommand() {
    }

/** Returns the name of this command */
    QString KEditMarkerCommand::name() const {
	if (m_create) {
	    return i18n("Edit Snap Marker");
	} else {
	    return i18n("Undo Edit Snap Marker");
	}
    }

/** Execute the command */
    void KEditMarkerCommand::execute() {
	if (m_create) {
	    addMarker();
	} else {
	    deleteMarker();
	}
	m_document.setModified(true);
    }

/** Unexecute the command */
    void KEditMarkerCommand::unexecute() {
	if (m_create) {
	    deleteMarker();
	} else {
	    addMarker();
	}
	m_document.setModified(true);
    }

    void KEditMarkerCommand::addMarker() {
	DocTrackBase *track = m_document.projectClip().track(m_track);

	if (track) {
	    DocClipRef *clip = track->getClipAt(m_trackTime);
	    if (clip) {
		QValueVector < CommentedTime > markers = clip->commentedSnapMarkers();
		QValueVector < CommentedTime >::iterator itt = markers.begin();
		while (itt != markers.end()) {
		    if ((*itt).time() == m_clipTime) break;
		    ++itt;
		}
		if (itt != markers.end()) {
		    m_previousComment = (*itt).comment();
		    clip->editSnapMarker(m_clipTime, m_comment);
		} else {
		kdError() <<
		    "Cannot find Marker..."
		    << endl;
	    }
	    } else {
		kdError() <<
		    "Trying to edit marker; no clip exists at this point on the track!"
		    << endl;
	    }

	} else {
	    kdError() <<
		"KEditMarkerCommand : trying to add marker, specified track "
		<< m_track << " does not exist in the project!" << endl;
	}
    }

    void KEditMarkerCommand::deleteMarker() {
	DocTrackBase *track = m_document.projectClip().track(m_track);

	if (track) {
	    DocClipRef *clip = track->getClipAt(m_trackTime);
	    if (clip) {
		QValueVector < CommentedTime > markers = clip->commentedSnapMarkers();
		QValueVector < CommentedTime >::iterator itt = markers.begin();
		while (itt != markers.end()) {
		    if ((*itt).time() == m_clipTime) break;
		    ++itt;
		}
		if (itt != markers.end()) {
		    clip->editSnapMarker(m_clipTime, m_previousComment);
		} else {
		kdError() <<
		    "Cannot find Marker..."
		    << endl;
	    }
	    } else {
		kdError() <<
		    "Trying to edit marker; no clip exists at this point on the track!"
		    << endl;
	    }

	} else {
	    kdError() <<
		"KEditMarkerCommand : trying to add marker, specified track "
		<< m_track << " does not exist in the project!" << endl;
	}

    }

}
