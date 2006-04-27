/***************************************************************************
                          kselectclipcommand.cpp  -  description
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

#include "kselectclipcommand.h"
#include "kdenlivedoc.h"
#include "docclipref.h"
#include "doctrackbase.h"
#include "doctrackclipiterator.h"

#include <kcommand.h>
#include <klocale.h>
#include <kdebug.h>

namespace Command {

    KSelectClipCommand::KSelectClipCommand(KdenliveDoc * doc,
	DocClipRef * clip, bool select):m_doc(doc) {
	DocTrackBase *docTrack = doc->findTrack(clip);
	 m_track = doc->trackIndex(docTrack);
	 m_findTime = clip->trackStart() + (clip->cropDuration() / 2.0);
	 m_selectClip = select;
	 m_unexecuteSelection = docTrack->clipSelected(clip);
    } 
    
    KSelectClipCommand::~KSelectClipCommand() {
    }

/** Executes the command */
    void KSelectClipCommand::execute() {
	DocTrackBase *track = m_doc->track(m_track);
	track->selectClip(track->getClipAt(m_findTime), m_selectClip);
    }

/** Unexecutes the command */
    void KSelectClipCommand::unexecute() {
	DocTrackBase *track = m_doc->track(m_track);
	track->selectClip(track->getClipAt(m_findTime),
	    m_unexecuteSelection);
    }

/** Rerturns the (translated) name of this command */
    QString KSelectClipCommand::name() const {
	return i18n("Selection");
    } 
    
    KCommand *KSelectClipCommand::selectNone(KdenliveDoc * document) {
	KMacroCommand *command = new KMacroCommand(i18n("Selection"));

	QPtrListIterator < DocTrackBase > trackItt(document->trackList());
	while (trackItt.current()) {
	    QPtrListIterator < DocClipRef >
		clipItt(trackItt.current()->firstClip(true));
	    while (clipItt.current() != 0) {
		Command::KSelectClipCommand * clipComm =
		    new Command::KSelectClipCommand(document,
		    clipItt.current(), false);
		command->addCommand(clipComm);
		++clipItt;
	    }
	    ++trackItt;
	}

	return command;
    }

// static
    KCommand *KSelectClipCommand::selectLaterClips(KdenliveDoc * document,
	GenTime time, bool include) {
	KMacroCommand *command = new KMacroCommand(i18n("Selection"));

	bool select;

	QPtrListIterator < DocTrackBase > trackItt(document->trackList());
	while (trackItt.current()) {
	    DocTrackClipIterator clipItt(*(trackItt.current()));
	    while (clipItt.current() != 0) {
		if (include) {
		    select = clipItt.current()->trackEnd() > time;
		} else {
		    select = clipItt.current()->trackStart() > time;
		}
		Command::KSelectClipCommand * clipComm =
		    new Command::KSelectClipCommand(document,
		    clipItt.current(), select);
		command->addCommand(clipComm);
		++clipItt;
	    }

	    ++trackItt;
	}

	return command;
    }

    KCommand *KSelectClipCommand::selectClipAt(KdenliveDoc * document,
	const DocTrackBase & track, const GenTime & value) {
	Command::KSelectClipCommand * command = 0;

	DocClipRef *clip = track.getClipAt(value);
	if (clip) {
	    command =
		new Command::KSelectClipCommand(document, clip, true);
	}

	return command;
    }

// static
    KCommand *KSelectClipCommand::toggleSelectClipAt(KdenliveDoc *
	document, const DocTrackBase & track, const GenTime & value) {
	KCommand *command = 0;
	DocClipRef *clip = track.getClipAt(value);
	if (clip) {
	    command =
		new Command::KSelectClipCommand(document, clip,
		!track.clipSelected(clip));
	}

	return command;
    }


}				// namespace Command
