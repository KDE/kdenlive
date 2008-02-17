/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include <KLocale>

#include "addtimelineclipcommand.h"

AddTimelineClipCommand::AddTimelineClipCommand(CustomTrackView *view, QDomElement xml, int clipId, int track, int startpos, QRectF rect, int duration, bool doIt, bool doRemove)
         : m_view(view), m_xml(xml), m_clipId(clipId), m_clipTrack(track), m_clipPos(startpos), m_clipRect(rect), m_clipDuration(duration), m_doIt(doIt), m_remove(doRemove) {
	    if (!m_remove) setText(i18n("Add timeline clip"));
	    else setText(i18n("Delete timeline clip"));
	 }


// virtual 
void AddTimelineClipCommand::undo()
{
  if (!m_remove) m_view->deleteClip(m_clipTrack, m_clipPos, m_clipRect);
  else m_view->addClip(m_xml, m_clipId, m_clipTrack, m_clipPos, m_clipRect, m_clipDuration);
}
// virtual 
void AddTimelineClipCommand::redo()
{
  if (m_doIt) {
    if (!m_remove) m_view->addClip(m_xml, m_clipId, m_clipTrack, m_clipPos, m_clipRect, m_clipDuration);
    else m_view->deleteClip(m_clipTrack, m_clipPos, m_clipRect);
  }
  m_doIt = true;
}

#include "addtimelineclipcommand.moc"
