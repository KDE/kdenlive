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
#include "kdenlivedoc.h"

namespace Command {

// static
KAddEffectCommand *KAddEffectCommand::appendEffect(KdenliveDoc *document, DocClipRef *clip, Effect *effect)
{
	return new KAddEffectCommand(document, clip, clip->numEffects(), effect);
}

KAddEffectCommand::KAddEffectCommand( KdenliveDoc *document, DocClipRef *clip, int effectIndex, Effect *effect) :
									KCommand(),
									m_addEffect(true),
									m_effect(effect->toXML()),
									m_trackIndex(clip->trackNum()),
									m_position(clip->trackStart() + clip->cropDuration()/2),
									m_effectIndex(effectIndex),
									m_document(document)
{
}

KAddEffectCommand::KAddEffectCommand( KdenliveDoc *document, DocClipRef *clip, int effectIndex) :
									KCommand(),
									m_addEffect(false),
									m_effect(clip->effectAt(effectIndex)->toXML()),
									m_trackIndex(clip->trackNum()),
									m_position(clip->trackStart() + clip->cropDuration()/2),
									m_effectIndex(effectIndex),
									m_document(document)
{
}

KAddEffectCommand::~KAddEffectCommand()
{
}

// virtual
QString KAddEffectCommand::name() const
{
	return m_addEffect ? i18n("Add effect") : i18n("Delete effect");
}

void KAddEffectCommand::execute()
{
	if(m_addEffect) {
		addEffect();
	} else {
		deleteEffect();
	}
}

void KAddEffectCommand::unexecute()
{
	if(m_addEffect) {
		deleteEffect();
	} else {
		addEffect();
	}
}

void KAddEffectCommand::addEffect()
{
	DocTrackBase *track = m_document->projectClip().track(m_trackIndex);
	if(track) {
		DocClipRef *clip = track->getClipAt(m_position);
		if(clip) {
			clip->addEffect(m_effectIndex, m_document->createEffect(m_effect.documentElement()));
		} else {
			kdError() << "KAddEffectCommand::addEffect() - cannot find clip at position " << m_position.seconds() << " on track " << m_trackIndex << endl;
		}
	} else {
		kdError() << "KAddEffectCommand::addEffect() - cannot find track index " << m_trackIndex << ", expect inconsistancies..." << endl;
	}
}

void KAddEffectCommand::deleteEffect()
{
	DocTrackBase *track = m_document->projectClip().track(m_trackIndex);
	if(track) {
		DocClipRef *clip = track->getClipAt(m_position);
		if(clip) {
			clip->deleteEffect(m_effectIndex);
		} else {
			kdError() << "KAddEffectCommand::deleteEffect() - cannot find clip at position " << m_position.seconds() << " on track " << m_trackIndex << endl;
		}
	} else {
		kdError() << "KAddEffectCommand::deleteEffect() - cannot find track index " << m_trackIndex << ", expect inconsistancies..." << endl;
	}
}

};
