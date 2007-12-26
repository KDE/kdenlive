/***************************************************************************
                          kaddeffectcommand  -  description
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
#include "kaddeffectcommand.h"

#include <qdom.h>

#include <kdebug.h>
#include <klocale.h>

#include "docclipproject.h"
#include "docclipref.h"
#include "doctrackbase.h"
#include "effect.h"
#include "effectparameter.h"
#include "effectparamdesc.h"
#include "kdenlivedoc.h"

namespace Command {

// static
    KAddEffectCommand *KAddEffectCommand::appendEffect(KdenliveDoc *
	document, DocClipRef * clip, Effect * effect) {
	return new KAddEffectCommand(document, clip, clip->numEffects(),
	    effect);
    }
// static
	KAddEffectCommand *KAddEffectCommand::
	insertEffect(KdenliveDoc * document, DocClipRef * clip,
	int effectIndex, Effect * effect) {
	return new KAddEffectCommand(document, clip, effectIndex, effect);
    }

// static
    KAddEffectCommand *KAddEffectCommand::removeEffect(KdenliveDoc *
	document, DocClipRef * clip, int effectIndex) {
	return new KAddEffectCommand(document, clip, effectIndex);
    }

// static
    KCommand *KAddEffectCommand::moveEffect(KdenliveDoc * document,
	DocClipRef * clip, int effectIndex, int newEffectIndex) {
	KMacroCommand *command = new KMacroCommand(i18n("Move Effect"));

	command->addCommand(removeEffect(document, clip, effectIndex));
	command->addCommand(insertEffect(document, clip, newEffectIndex,
		clip->effectStack()[effectIndex]));
	return command;
    }

  KAddEffectCommand::KAddEffectCommand(KdenliveDoc * document, DocClipRef * clip, int effectIndex, Effect * effect):
    KCommand(),
	m_addEffect(true),
	m_effect(effect->toXML()),
	m_trackIndex(clip->trackNum()),
	m_position(clip->trackMiddleTime()),
	m_effectIndex(effectIndex), m_document(document) {
    }

    KAddEffectCommand::KAddEffectCommand(KdenliveDoc * document,
	DocClipRef * clip, int effectIndex):KCommand(), m_addEffect(false),
	m_effect(clip->effectAt(effectIndex)->toXML()),
	m_trackIndex(clip->trackNum()),
	m_position(clip->trackMiddleTime()),
	m_effectIndex(effectIndex), m_document(document) {
    }

    KAddEffectCommand::~KAddEffectCommand() {
    }

// virtual
    QString KAddEffectCommand::name() const {
	return m_addEffect ? i18n("Add effect") : i18n("Delete effect");
    } void KAddEffectCommand::execute() {
	if (m_addEffect) {
	    addEffect();
	} else {
	    deleteEffect();
	}
    }

    void KAddEffectCommand::unexecute() {
	if (m_addEffect) {
	    deleteEffect();
	} else {
	    addEffect();
	}
    }

    void KAddEffectCommand::addEffect() {
	DocTrackBase *track =
	    m_document->projectClip().track(m_trackIndex);
	if (track) {

	    Effect *effect = m_document->createEffect(m_effect.documentElement());
	    track->addEffectToClip(m_position, m_effectIndex, effect);
	    DocClipRef *clip = track->getClipAt(m_position);
	    if (!clip) {
		kdDebug()<<"// CANNOT FIND CLIP TO APPLY EFFECT"<<endl;
		return;
	    }
	    QMap <QString, QString> params = effect->getParameters(clip);
	    QString tag = effect->effectDescription().tag();

	    if (tag != QString("framebuffer")) {
		m_document->renderer()->mltAddEffect(m_document->projectClip().playlistTrackNum(m_trackIndex), m_position, effect->effectDescription().stringId(), tag, params);

		if (tag == QString ("chroma") || tag == QString("affine")) {
		    QMap<QString, QString> params;
		    params["fill"] = "1";
		    params["progressive"] = "1";
		    params["valign"] = "1";
		    params["halign"] = "1";
		    int endtrack = m_document->projectClip().playlistTrackNum(m_trackIndex);
		    int track = m_document->projectClip().playlistNextVideoTrack(m_trackIndex);
		    m_document->renderer()->mltAddTransition("composite", track, endtrack, clip->trackStart(), clip->trackEnd(), params);
		}
	    }
	    else m_document->activateSceneListGeneration(true);
	} else {
	    kdError() <<
		"KAddEffectCommand::addEffect() - cannot find track index "
		<< m_trackIndex << ", expect inconsistancies..." << endl;
	}
    }

    void KAddEffectCommand::deleteEffect() {
	DocTrackBase *track =
	    m_document->projectClip().track(m_trackIndex);
	if (track) {
	    Effect *effect = m_document->createEffect(m_effect.documentElement());
	    QString tag = effect->effectDescription().tag();
	    track->deleteEffectFromClip(m_position, m_effectIndex);
	    int index = m_effectIndex;
	    if (effect->effectDescription().parameter(0)->type() == "complex" || effect->effectDescription().parameter(0)->type() == "double") index = -1;
	    if (effect->isEnabled()) {
		if (tag != QString("chroma") && tag != QString("affine")) 
		    m_document->renderer()->mltRemoveEffect(m_document->projectClip().playlistTrackNum(m_trackIndex), m_position, effect->effectDescription().stringId(),  tag, index);
		else m_document->activateSceneListGeneration(true);
	    }
	    delete effect;
	} else {
	    kdError() <<
		"KAddEffectCommand::deleteEffect() - cannot find track index "
		<< m_trackIndex << ", expect inconsistancies..." << endl;
	}
    }

}
