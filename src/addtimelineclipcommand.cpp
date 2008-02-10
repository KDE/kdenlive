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

AddTimelineClipCommand::AddTimelineClipCommand(CustomTrackView *view, QDomElement xml, int track, int startpos, QRectF rect, int duration, bool doIt)
         : m_view(view), m_xml(xml), m_clipTrack(track), m_clipPos(startpos), m_clipRect(rect), m_clipDuration(duration), m_doIt(doIt) {
	    setText(i18n("Add timeline clip"));
	 }


// virtual 
void AddTimelineClipCommand::undo()
{
// kDebug()<<"----  undoing action";
  m_doIt = true;
  if (m_doIt) m_view->deleteClip(m_clipTrack, m_clipPos, m_clipRect);
}
// virtual 
void AddTimelineClipCommand::redo()
{
  //kDebug()<<"----  redoing action";
  if (m_doIt) m_view->addClip(m_xml, m_clipTrack, m_clipPos, m_clipRect, m_clipDuration);
  m_doIt = true;
}

#include "addtimelineclipcommand.moc"
