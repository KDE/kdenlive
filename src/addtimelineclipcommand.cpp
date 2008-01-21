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

#include "addtimelineclipcommand.h"

AddTimelineClipCommand::AddTimelineClipCommand(CustomTrackView *view, int clipType, QString clipName, int clipProducer, QRectF rect, bool doIt)
         : m_view(view), m_clipType(clipType), m_clipName(clipName), m_clipProducer(clipProducer), m_clipRect(rect), m_doIt(doIt) {
	    setText(i18n("Add timeline clip"));
	 }


// virtual 
void AddTimelineClipCommand::undo()
{
// kDebug()<<"----  undoing action";
  m_doIt = true;
  if (m_doIt) m_view->deleteClip(m_clipRect);
}
// virtual 
void AddTimelineClipCommand::redo()
{
  //kDebug()<<"----  redoing action";
  if (m_doIt) m_view->addClip(m_clipType, m_clipName, m_clipProducer, m_clipRect);
  m_doIt = true;
}

#include "addtimelineclipcommand.moc"
