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

KResizeCommand::KResizeCommand(KdenliveDoc *doc, DocClipBase *clip)
{
	m_doc = doc;
	m_trackNum = clip->trackNum();
	m_start_cropDuration = clip->cropDuration();
	m_start_trackStart = clip->trackStart();
	m_start_cropStart = clip->cropStartTime();
}

KResizeCommand::~KResizeCommand()
{
}

void KResizeCommand::setEndSize(DocClipBase *clip)
{
	m_end_cropDuration = clip->cropDuration();
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
	DocClipBase *clip = m_doc->track(m_trackNum)->getClipAt( m_start_trackStart + (m_start_cropDuration / 2.0));
	clip->setTrackStart(m_end_trackStart);
	clip->setCropStartTime(m_end_cropStart);
	clip->setCropDuration(m_end_cropDuration);	
}

/** Unexecutes this command */
void KResizeCommand::unexecute()
{
	DocClipBase *clip = m_doc->track(m_trackNum)->getClipAt( m_end_trackStart + (m_end_cropDuration / 2.0));
	clip->setTrackStart(m_start_trackStart);
	clip->setCropStartTime(m_start_cropStart);
	clip->setCropDuration(m_start_cropDuration);
}
