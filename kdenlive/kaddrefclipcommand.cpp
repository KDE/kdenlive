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
	KdenliveDoc & document, DocClipRef * clip, bool create):m_document(document),
	m_effectList(effectList), m_create(create),
	m_xmlClip(clip->toXML()),
	m_findTime(clip->trackStart() + (clip->cropDuration() / 2.0)),
	m_track(clip->trackNum()) {
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
	    DocClipRef::createClip(m_effectList, m_document.clipManager(),
	    m_xmlClip.documentElement());

        clip->referencedClip()->addReference();
	if (clip->referencedClip()->numReferences() == 1) m_document.generateProducersList();
	m_document.projectClip().track(clip->trackNum())->addClip(clip, true);
        m_document.projectClip().slotClipReferenceChanged();
    }

    void KAddRefClipCommand::deleteClip() {
	DocTrackBase *track = m_document.projectClip().track(m_track);
	if (!track) {
		kdDebug()<<"///////// WARNING, TRYING TO DELETE CLIP ON DEAD TRACK "<<m_track<<endl;
		return;
	}
	DocClipRef *clip = track->getClipAt(m_findTime);
	if (!clip) {
		kdDebug()<<"///////// WARNING, TRYING TO DELETE DEAD CLIP AT "<<m_findTime.frames(25)<<" ON TRACK: "<<m_track<<endl;
		return;
	}
	track->removeClip(clip);
        clip->referencedClip()->removeReference();
	if (clip->referencedClip()->numReferences() == 0) m_document.generateProducersList();
        m_document.projectClip().slotClipReferenceChanged();
	delete clip;
    }

// static
    KMacroCommand *KAddRefClipCommand::deleteSelectedClips(KdenliveDoc *
	document) {
	KMacroCommand *macroCommand =
	    new KMacroCommand(i18n("Delete Clips"));

	for (uint count = 0; count < document->numTracks(); ++count) {
	    DocTrackBase *track = document->track(count);
	    uint found = 0;
	    QPtrListIterator < DocClipRef > itt = track->firstClip(true);

	    while (itt.current()) {
		Command::KAddRefClipCommand * command =
		    new Command::KAddRefClipCommand(document->
		    effectDescriptions(), *document, itt.current(), false);
		macroCommand->addCommand(command);
		found++;
		++itt;
	    }
	    //kdDebug()<<" -----------  FOUND: "<<found<<" selected clips on track: "<<count<<endl;
	}

	return macroCommand;
    }

    KMacroCommand *KAddRefClipCommand::deleteAllTrackClips(KdenliveDoc *
	document, int ix) {
	KMacroCommand *macroCommand = new KMacroCommand(i18n("Delete Clips"));
	
	    DocTrackBase *track = document->track(ix);

	    QPtrListIterator < DocClipRef > itt = track->firstClip(true);
	    while (itt.current()) {
		Command::KAddRefClipCommand * command =
		    new Command::KAddRefClipCommand(document->
		    effectDescriptions(), *document, itt.current(), false);
		macroCommand->addCommand(command);
		++itt;
	    }

	    itt = track->firstClip(false);
	    while (itt.current()) {
		Command::KAddRefClipCommand * command =
		    new Command::KAddRefClipCommand(document->
		    effectDescriptions(), *document, itt.current(), false);
		macroCommand->addCommand(command);
		++itt;
	    }

	return macroCommand;
    }

}				// namespace command
