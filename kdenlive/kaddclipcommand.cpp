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

#include "kaddclipcommand.h"
#include "kdenlivedoc.h"
#include "docclipbase.h"

namespace Command {

/** Construct an AddClipCommand that will add or delete a clip */
KAddClipCommand::KAddClipCommand(KdenliveDoc *doc, DocClipBase *clip, bool create)
{
	m_doc = doc;
	m_xmlClip = clip->toXML().documentElement();
	m_track = clip->trackNum();
	m_findTime = clip->trackStart() + (clip->cropDuration() / 2.0);
	m_create = create;
}

KAddClipCommand::~KAddClipCommand()
{
}

/** Returns the name of this command */
QString KAddClipCommand::name() const
{
	if(m_create) {
		return i18n("Add Clip");
	} else {
		return i18n("Delete Clip");
	}
}

/** Execute the command */
void KAddClipCommand::execute()
{
	if(m_create) {
		addClip();
	} else {
		deleteClip();
	}
}

/** Unexecute the command */
void KAddClipCommand::unexecute()
{
	if(m_create) {
		deleteClip();
	} else {
		addClip();
	}
}

/** Adds the clip */
void KAddClipCommand::addClip()
{
	DocClipBase *clip = DocClipBase::createClip(m_doc, m_xmlClip);
	m_doc->track(clip->trackNum())->addClip(clip, true);
}

/** Deletes the clip */
void KAddClipCommand::deleteClip()
{
	DocTrackBase *track = m_doc->track(m_track);
	DocClipBase *clip = track->getClipAt(m_findTime);
	track->removeClip(clip);
	
	delete clip;	
}

} // namespace command

