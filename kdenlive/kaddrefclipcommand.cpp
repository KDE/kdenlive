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

#include "kaddrefclipcommand.h"
#include "kdenlivedoc.h"
#include "docclipbase.h"
#include "clipmanager.h"
#include "docclipproject.h"

namespace Command {

    KAddRefClipCommand::
	KAddRefClipCommand(const EffectDescriptionList & effectList,
	ClipManager & clipManager, DocClipProject * project,
	DocClipRef * clip, bool create):m_clipManager(clipManager),
	m_effectList(effectList), m_create(create),
	m_xmlClip(clip->toXML()),
	m_findTime(clip->trackStart() + (clip->cropDuration() / 2.0)),
	m_track(clip->trackNum()), m_project(project) {
    } 
    
    KAddRefClipCommand::~KAddRefClipCommand() {
    }

    QString KAddRefClipCommand::name() const {
	if (m_create) {
	    return i18n("Add Clip");
	} else {
	    return i18n("Delete Clip");
	}
    }

    void KAddRefClipCommand::execute() {
	if (m_create) {
	    addClip();
	} else {
	    deleteClip();
	}
    }

    void KAddRefClipCommand::unexecute() {
	if (m_create) {
	    deleteClip();
	} else {
	    addClip();
	}
    }

    void KAddRefClipCommand::addClip() {
	DocClipRef *clip =
	    DocClipRef::createClip(m_effectList, m_clipManager,
	    m_xmlClip.documentElement());
        //if (clip->hasVariableThumbnails()) clip->generateThumbnails();
        clip->referencedClip()->addReference();
	m_project->track(clip->trackNum())->addClip(clip, true);
        m_project->slotClipReferenceChanged();
    }

    void KAddRefClipCommand::deleteClip() {
	DocTrackBase *track = m_project->track(m_track);
	DocClipRef *clip = track->getClipAt(m_findTime);
        m_project->deleteClipTransition( clip );
	track->removeClip(clip);
        clip->referencedClip()->removeReference();
        m_project->slotClipReferenceChanged();
	delete clip;
    }

// static
    KMacroCommand *KAddRefClipCommand::deleteSelectedClips(KdenliveDoc *
	document) {
	KMacroCommand *macroCommand =
	    new KMacroCommand(i18n("Delete Clips"));

	for (uint count = 0; count < document->numTracks(); ++count) {
	    DocTrackBase *track = document->track(count);

	    QPtrListIterator < DocClipRef > itt = track->firstClip(true);

	    while (itt.current()) {
		Command::KAddRefClipCommand * command =
		    new Command::KAddRefClipCommand(document->
		    effectDescriptions(), document->clipManager(),
		    &document->projectClip(), itt.current(), false);
		macroCommand->addCommand(command);
		++itt;
	    }
	}

	return macroCommand;
    }

}				// namespace command
