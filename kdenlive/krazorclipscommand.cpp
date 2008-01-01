/***************************************************************************
                          krazorclipscommand.cpp  -  description
                             -------------------

   copyright            : (C) 2007 by Jean-Baptiste Mardelle
   email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <klocale.h>
#include <kdebug.h>

#include "krazorclipscommand.h"
#include "kdenlivedoc.h"
#include "docclipproject.h"

#include "kaddrefclipcommand.h"
#include "kresizecommand.h"
#include "kselectclipcommand.h"
#include "kaddtransitioncommand.h"

namespace Command {

    KRazorClipsCommand::KRazorClipsCommand(Gui::KdenliveApp * app, KdenliveDoc * doc, DocTrackBase & track,
	GenTime time) :KCommand(), m_app(app), m_doc(doc), m_splitTime(time) {
	DocClipRef *clip = track.getClipAt(time);
	m_playlistTrackNum = clip->playlistTrackNum();
	m_trackNum = clip->trackNum();
	m_startTime = clip->trackStart();
	m_endTime = clip->trackEnd();
	m_cropStart = clip->cropStartTime();
    }

    KRazorClipsCommand::~KRazorClipsCommand() {
    }

// virtual
    QString KRazorClipsCommand::name() const {
	return i18n("Razor Clip");
    }

/** Executes this command */
    void KRazorClipsCommand::execute() {
	DocClipRef *clip = m_doc->track(m_trackNum)->getClipAt(m_splitTime);
	if (clip) {
	    // disallow the creation of clips with 0 length.
	    if ((clip->trackStart() == m_splitTime) || (clip->trackEnd() == m_splitTime))
		return;

	    m_doc->renderer()->mltCutClip(m_playlistTrackNum, m_splitTime);
	    KMacroCommand *command = new KMacroCommand(i18n("Razor clip"));

	    DocClipRef *clone = clip->clone(m_doc);
	    if (clone) {
		clip->setTrackEnd(m_splitTime);
	        clone->moveCropStartTime(clip->cropStartTime() + (m_splitTime - clip->trackStart()));
	        clone->setTrackStart(m_splitTime);
	        // Remove original clip's transitions that are after the cut
	        TransitionStack transitionStack = clip->clipTransitions();
	        if (!transitionStack.isEmpty()) {
    		    TransitionStack::iterator itt = transitionStack.begin();
    		    while (itt) {
        	        if ((*itt)->transitionStartTime()>=m_splitTime) {
			    Command::KAddTransitionCommand * remTransitionCommand = new Command::KAddTransitionCommand(m_doc, clip, *itt, false);
			    command->addCommand(remTransitionCommand);
		        }
        	        ++itt;
    		    }
	     	}
		

		clone->referencedClip()->addReference();
		m_doc->projectClip().track(clone->trackNum())->addClip(clone, true);
        	m_doc->projectClip().slotClipReferenceChanged();
		command->addCommand(Command::KSelectClipCommand::selectNone(m_doc));
		m_app->addCommand(command, true);
	    }
	}
    }

/** Unexecute this command */
    void KRazorClipsCommand::unexecute() {
	m_doc->renderer()->mltRemoveClip(m_playlistTrackNum, m_splitTime + GenTime(1, m_doc->framesPerSecond()));
	m_doc->renderer()->mltResizeClipEnd(m_playlistTrackNum, m_startTime, m_cropStart, m_cropStart + m_endTime - m_startTime);
	DocClipRef *oldClip = m_doc->track(m_trackNum)->getClipAt(m_splitTime + GenTime(1, m_doc->framesPerSecond()));
	if (oldClip) m_doc->track(m_trackNum)->removeClip(oldClip);
	DocClipRef *originalClip = m_doc->track(m_trackNum)->getClipAt(m_splitTime - GenTime(1, m_doc->framesPerSecond()));

	if (originalClip) {
	    // disallow the creation of clips with 0 length.
		originalClip->setTrackEnd(m_endTime);
		originalClip->referencedClip()->removeReference();
        	m_doc->projectClip().slotClipReferenceChanged();
		//command->addCommand(Command::KSelectClipCommand::selectNone(m_doc));
	}
    }

}				// namespace Command
