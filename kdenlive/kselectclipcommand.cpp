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
#include "docclipbase.h"
#include <klocale.h>
#include <kdebug.h>

KSelectClipCommand::KSelectClipCommand(KdenliveDoc *doc, DocClipBase *clip, bool select)
{
	m_doc = doc;
	DocTrackBase *docTrack = doc->findTrack(clip);
	m_track = doc->trackIndex(docTrack);
	m_findTime = clip->trackStart() + (clip->cropDuration() / 2.0);
	m_selectClip = select;
	m_unexecuteSelection = docTrack->clipSelected(clip);
}

KSelectClipCommand::~KSelectClipCommand()
{
}

/** Executes the command */
void KSelectClipCommand::execute()
{
	DocTrackBase *track = m_doc->track(m_track);		
	track->selectClip(track->getClipAt(m_findTime), m_selectClip);
}

/** Unexecutes the command */
void KSelectClipCommand::unexecute()
{
	DocTrackBase *track = m_doc->track(m_track);
	track->selectClip(track->getClipAt(m_findTime), m_unexecuteSelection);
}

/** Rerturns the (translated) name of this command */
QString KSelectClipCommand::name() const
{
	return i18n("Selection");
}
