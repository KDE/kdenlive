/***************************************************************************
                          kaddclipcommand.cpp  -  description
                             -------------------
    begin                : Fri Dec 13 2002
    copyright            : (C) 2002 by Jason Wood
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

#include <kdebug.h>

#include "keditclipcommand.h"
#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "clipmanager.h"
#include "docclipproject.h"
#include "docclipavfile.h"
#include "documentbasenode.h"
#include "documentclipnode.h"
#include "documentgroupnode.h"
#include "projectlist.h"

namespace Command {



/** Edit Color clip */
    KEditClipCommand::KEditClipCommand(KdenliveDoc & document,
	DocClipRef * clip, const QString & color, const GenTime & duration,
	const QString & name,
	const QString & description):m_document(document),
	m_name("Color Clip"), m_parent(document.clipHierarch()->name()) {
	if (!m_parent) {
	    kdWarning() <<
		"Error - all clips created with kaddclipcommand should have a parent!"
		<< endl;
	}
	document.clipManager().editColorClip(clip, color, duration, name,
	    description);

/*	DocumentClipNode *clipNode = new DocumentClipNode(0, clip);
	m_xmlClip = clipNode->clipRef()->toXML();
	delete clipNode;*/

    }


/** Edit Image clip */
    KEditClipCommand::KEditClipCommand(KdenliveDoc & document,
	DocClipRef * clip, const KURL & url, const QString & extension,
	const int &ttl, const GenTime & duration,
	const QString & description):m_document(document),
	m_name(url.filename()), m_parent(document.clipHierarch()->name()) {
	if (!m_parent) {
	    kdWarning() <<
		"Error - all clips created with kaddclipcommand should have a parent!"
		<< endl;
	}

	document.clipManager().editImageClip(clip, url, extension, ttl,
	    duration, description);

/*	DocumentClipNode *clipNode = new DocumentClipNode(0, clip);
	m_xmlClip = clipNode->clipRef()->toXML();
	delete clipNode;*/
    }



/** Edit AUDIO/VIDEO clip */
    KEditClipCommand::KEditClipCommand(KdenliveDoc & document,
	DocClipRef * clip, const KURL & url):m_document(document),
	m_name(url.filename()), m_parent(document.clipHierarch()->name()) {
	if (!m_parent) {
	    kdWarning() <<
		"Error - all clips created with kaddclipcommand should have a parent!"
		<< endl;
	}

	document.clipManager().editClip(clip, url);

/*	DocumentClipNode *clipNode = new DocumentClipNode(0, clip);
	m_xmlClip = clipNode->clipRef()->toXML();
	delete clipNode;*/
    }

    KEditClipCommand::~KEditClipCommand() {
    }

/** Returns the name of this command */
    QString KEditClipCommand::name() const {
	return i18n("Edit Clip");
    }
/** Execute the command */ void KEditClipCommand::execute() {
	addClip();
    }

/** Execute the command */
    void KEditClipCommand::unexecute() {
	//addClip();
    }

/** Adds the clip */
    void KEditClipCommand::addClip() {
	DocumentBaseNode *node = m_document.findClipNode(m_parent);

	if (!node) {
	    kdWarning() <<
		"Could not find parent in document, cannot add document base node"
		<< endl;
	} else {
	    DocClipBase *clip =
		m_document.clipManager().insertClip(m_xmlClip.
		documentElement());
	    if (clip)
		m_document.addClipNode(m_parent, new DocumentClipNode(node,
			clip));
	}
    }

}				// namespace command
