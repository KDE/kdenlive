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

/** Construct an AddClipCommand that will add or delete a clip */
KAddRefClipCommand::KAddRefClipCommand(ClipManager &clipManager,
					DocClipProject *project, 
					DocClipRef *clip, 
					bool create) :
			m_clipManager(clipManager),
			m_create(create),
			m_xmlClip(clip->toXML().documentElement()),
			m_findTime(clip->trackStart() + (clip->cropDuration() / 2.0)),
			m_track(clip->trackNum()),
			m_project(project)
{
}

KAddRefClipCommand::~KAddRefClipCommand()
{
}

/** Returns the name of this command */
QString KAddRefClipCommand::name() const
{
	if(m_create) {
		return i18n("Add Clip");
	} else {
		return i18n("Delete Clip");
	}
}

/** Execute the command */
void KAddRefClipCommand::execute()
{
	if(m_create) {
		addClip();
	} else {
		deleteClip();
	}
}

/** Unexecute the command */
void KAddRefClipCommand::unexecute()
{
	if(m_create) {
		deleteClip();
	} else {
		addClip();
	}
}

/** Adds the clip */
void KAddRefClipCommand::addClip()
{
	DocClipRef *clip = DocClipRef::createClip(m_clipManager, m_xmlClip);
	m_project->track(clip->trackNum())->addClip(clip, true);
}

/** Deletes the clip */
void KAddRefClipCommand::deleteClip()
{
	DocTrackBase *track = m_project->track(m_track);
	DocClipRef *clip = track->getClipAt(m_findTime);
	track->removeClip(clip);
	
	delete clip;	
}

} // namespace command

