/***************************************************************************
                          kmoveclipscommand.cpp  -  description
                             -------------------
    begin                : Thu Dec 12 2002
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

#include "kmoveclipscommand.h"
#include "kdenlivedoc.h"
#include <klocale.h>
#include <kdebug.h>

namespace Command {

KMoveClipsCommand::KMoveClipsCommand(KdenliveDoc *doc, DocClipRef *master)
													: KCommand(),
													m_doc(doc)
{
	m_startTrack = doc->trackIndex(doc->findTrack(master));
	if(m_startTrack == -1) {
		kdError() << "KMoveClipCommand created, but master clip is not in document!" << endl;
		return;
	}

	// Choose a "safe" time in the middle of the master clip, so that we are certain to be able to find it again when
	// we need to.
	m_startTime = master->trackStart();
}

KMoveClipsCommand::~KMoveClipsCommand()
{
}

// virtual
QString KMoveClipsCommand::name() const
{
	return i18n("Move Clips");
}

/** Specifies where the master clip should be moved to by this command. */
void KMoveClipsCommand::setEndLocation(DocClipRef *master)
{
	m_endTrack = m_doc->trackIndex(m_doc->findTrack(master));
	m_endTime = master->trackStart();
}

/** Executes this command */
void KMoveClipsCommand::execute()
{
	m_doc->moveSelectedClips(m_endTime - m_startTime, m_endTrack - m_startTrack);
}

/** Unexecute this command */
void KMoveClipsCommand::unexecute()
{
	m_doc->moveSelectedClips(m_startTime - m_endTime, m_startTrack - m_endTrack);
}

} // namespace Command
