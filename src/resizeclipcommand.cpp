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


#include "resizeclipcommand.h"
#include "customtrackview.h"

#include <KLocale>

ResizeClipCommand::ResizeClipCommand(CustomTrackView *view, const ItemInfo start, const ItemInfo end, bool doIt, QUndoCommand * parent) : QUndoCommand(parent), m_view(view), m_startPos(start), m_endPos(end), m_doIt(doIt)
{
    setText(i18n("Resize clip"));
}

// virtual
void ResizeClipCommand::undo()
{
// kDebug()<<"----  undoing action";
    m_doIt = true;
    if (m_doIt) m_view->resizeClip(m_endPos, m_startPos);
}
// virtual
void ResizeClipCommand::redo()
{
    kDebug() << "----  redoing action";
    if (m_doIt) m_view->resizeClip(m_startPos, m_endPos);
    m_doIt = true;
}

