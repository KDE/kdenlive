/***************************************************************************
                          KMoveTransitionCommand  -  description
                             -------------------
    begin                : Thu Jan 22 2004
    copyright            : (C) 2004 by Jason Wood
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
#include "kmovetransitioncommand.h"

#include <qdom.h>

#include <kdebug.h>
#include <klocale.h>

#include "docclipproject.h"
#include "docclipref.h"
#include "doctrackbase.h"
#include "transition.h"
#include "kdenlivedoc.h"

namespace Command {

  KMoveTransitionCommand::KMoveTransitionCommand(KdenliveDoc * document, DocClipRef * clip, Transition *transit, GenTime oldIn, GenTime oldOut, GenTime newIn, GenTime newOut):
    KCommand(),
	m_transitionOldStart(oldIn), m_transitionOldEnd(oldOut), m_transitionNewStart(newIn), m_transitionNewEnd(newOut), m_trackIndex(clip->trackNum()),
	m_position(clip->trackStart() + clip->cropDuration() / 2), m_document(document), m_transitionIndex(clip->transitionIndex(transit)) {
    }


    KMoveTransitionCommand::~KMoveTransitionCommand() {
    }

// virtual
    QString KMoveTransitionCommand::name() const {
	return i18n("Move transition");
    } 

    void KMoveTransitionCommand::execute() {
        moveTransition(true);
    }

    void KMoveTransitionCommand::unexecute() {
        moveTransition(false);
    }

    void KMoveTransitionCommand::moveTransition(bool revert) {
	DocTrackBase *track = m_document->projectClip().track(m_trackIndex);
	if (track) {
		DocClipRef *clip = track->getClipAt(m_position);
		if (!clip) return;
		Transition *tr = clip->transitionAt(m_transitionIndex);
		if (revert) {
			m_document->renderer()->mltMoveTransition(tr->transitionTag(), tr->transitionStartTrack(), 0, m_transitionOldStart, m_transitionOldEnd, m_transitionNewStart, m_transitionNewEnd);
		}
		else {
			m_document->renderer()->mltMoveTransition(tr->transitionTag(), tr->transitionStartTrack(), 0, m_transitionNewStart, m_transitionNewEnd, m_transitionOldStart, m_transitionOldEnd);
		}
	}
    }


}
