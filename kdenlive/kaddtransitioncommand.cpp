/***************************************************************************
                          KAddTransitionCommand  -  description
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
#include "kaddtransitioncommand.h"

#include <qdom.h>

#include <kdebug.h>
#include <klocale.h>

#include "docclipproject.h"
#include "docclipref.h"
#include "doctrackbase.h"
#include "transition.h"
#include "kdenlivedoc.h"

namespace Command {

// static
    KAddTransitionCommand *KAddTransitionCommand::appendTransition( DocClipRef * clip, GenTime time, const QString & type) {
        Transition *transit = new Transition(clip, time, type);
	if (transit) return new KAddTransitionCommand(clip, transit, true);
    }

// static
    KAddTransitionCommand *KAddTransitionCommand::appendTransition( DocClipRef * a_clip, DocClipRef * b_clip, const QString & type) {
        Transition *transit = new Transition(a_clip, b_clip, type);
	if (transit) return new KAddTransitionCommand(a_clip, transit, true);
    }

// static
    KAddTransitionCommand *KAddTransitionCommand::removeTransition( DocClipRef * clip, Transition *transit) {
	return new KAddTransitionCommand(clip, transit, false);
    }

// static
/*
    KCommand *KAddTransitionCommand::moveTransition(KdenliveDoc * document,
	DocClipRef * clip, int effectIndex, int newEffectIndex) {
	KMacroCommand *command = new KMacroCommand(i18n("Move Effect"));

	command->addCommand(removeEffect(document, clip, effectIndex));
	command->addCommand(insertEffect(document, clip, newEffectIndex,
		clip->effectStack()[effectIndex]));
	return command;
    }*/

  KAddTransitionCommand::KAddTransitionCommand(DocClipRef * clip, Transition *transit, bool add):
    KCommand(),
	m_addTransition(add), m_clip(clip),
	m_transition(transit->toXML()) {
    }


    KAddTransitionCommand::~KAddTransitionCommand() {
    }

// virtual
    QString KAddTransitionCommand::name() const {
	return m_addTransition ? i18n("Add transition") : i18n("Delete transition");
    } void KAddTransitionCommand::execute() {
	if (m_addTransition) {
	    addTransition();
	} else {
	    deleteTransition();
	}
    }

    void KAddTransitionCommand::unexecute() {
	if (m_addTransition) {
	    deleteTransition();
	} else {
	    addTransition();
	}
    }

    void KAddTransitionCommand::addTransition() {

	Transition *tr = new Transition(m_clip, m_transition);
	m_clip->addTransition(tr);
    }

    void KAddTransitionCommand::deleteTransition() {
	
	m_clip->deleteTransition(m_transition);
    }

}
