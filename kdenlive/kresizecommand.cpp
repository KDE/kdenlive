/***************************************************************************
                          kresizecommand.cpp  -  description
                             -------------------
    begin                : Sat Dec 14 2002
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

#include "kresizecommand.h"
#include "docclipbase.h"
#include "kdenlivedoc.h"

#include <klocale.h>
#include <kdebug.h>

namespace Command {

KResizeCommand::KResizeCommand(KdenliveDoc *doc, DocClipBase *clip)
{
	m_doc = doc;
	m_trackNum = clip->trackNum();
	m_end_trackEnd = m_start_trackEnd = clip->trackEnd();
	m_end_trackStart = m_start_trackStart = clip->trackStart();
	m_end_cropStart = m_start_cropStart = clip->cropStartTime();
}

KResizeCommand::~KResizeCommand()
{
}

void KResizeCommand::setEndSize(DocClipBase *clip)
{
	m_end_trackEnd = clip->trackEnd();
	m_end_trackStart = clip->trackStart();
	m_end_cropStart = clip->cropStartTime();
}

/** Returns the name of this command */
QString KResizeCommand::name() const
{
	return i18n("Resize clip");
}

/** Executes this command */
void KResizeCommand::execute()
{
	DocClipBase *clip = m_doc->track(m_trackNum)->getClipAt( (m_start_trackStart + m_start_trackEnd) / 2.0);
	if(!clip) {
		kdWarning() << "ResizeCommand execute failed - cannot find clip!!!" << endl;
	} else {
		clip->setTrackStart(m_end_trackStart);
		clip->setCropStartTime(m_end_cropStart);
		clip->setTrackEnd(m_end_trackEnd);
	}
}

/** Unexecutes this command */
void KResizeCommand::unexecute()
{
	DocClipBase *clip = m_doc->track(m_trackNum)->getClipAt( m_end_trackStart + ((m_end_trackEnd - m_end_trackStart) / 2.0));
	if(!clip) {
		kdWarning() << "ResizeCommand unexecute failed - cannot find clip!!!" << endl;
	} else {
		clip->setTrackStart(m_start_trackStart);
		clip->setCropStartTime(m_start_cropStart);
		clip->setTrackEnd(m_start_trackEnd);
	}
}

/** Sets the trackEnd() for the end destination to the time specified. */
void KResizeCommand::setEndTrackEnd(const GenTime &time)
{
	m_end_trackEnd = time;
}

void KResizeCommand::setEndTrackStart(const GenTime &time)
{
	m_end_trackStart = time;
}

} // namespace Command
