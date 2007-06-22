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
	int clipId,
	const GenTime & clipTime, QString comment,
	bool create):KCommand(),
	m_document(document),
	m_create(create),
	m_clipTime(clipTime), m_comment(comment), 
	m_id(clipId) {
    } 

    KAddMarkerCommand::~KAddMarkerCommand() {
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
	m_document.setModified(true);
    }

/** Unexecute the command */
    void KAddMarkerCommand::unexecute() {
	if (m_create) {
	    deleteMarker();
	} else {
	    addMarker();
	}
	m_document.setModified(true);
    }

    void KAddMarkerCommand::addMarker() {
	DocClipBase *clip = m_document.findClipById(m_id);
	if (clip) {
	    clip->addSnapMarker(m_clipTime, m_comment);
	    m_document.redrawTimeLine();
	} else {
	    kdError() <<"Trying to add marker; no clip exists at this point on the track!"<< endl;
	}
    }

    void KAddMarkerCommand::deleteMarker() {
	DocClipBase *clip = m_document.findClipById(m_id);
	if (clip) {
	    m_comment = clip->deleteSnapMarker(m_clipTime);
	    m_document.redrawTimeLine();
	} else {
	    kdError() <<"Trying to delete marker; no clip exists at this point on the track!"<< endl;
	}
    }

}
