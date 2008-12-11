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

#include "movegroupcommand.h"
#include "customtrackview.h"

MoveGroupCommand::MoveGroupCommand(CustomTrackView *view, const QList <ItemInfo> startClip, const QList <ItemInfo> startTransition, const GenTime offset, const int trackOffset, bool doIt, QUndoCommand * parent) : QUndoCommand(parent), m_view(view), m_startClip(startClip), m_startTransition(startTransition), m_offset(offset), m_trackOffset(trackOffset), m_doIt(doIt) {
    setText(i18n("Move clip"));
}


// virtual
void MoveGroupCommand::undo() {
// kDebug()<<"----  undoing action";
    m_doIt = true;
    m_view->moveGroup(m_startClip, m_startTransition, GenTime() - m_offset, - m_trackOffset, true);
}
// virtual
void MoveGroupCommand::redo() {
    kDebug() << "----  redoing action";
    if (m_doIt)
        m_view->moveGroup(m_startClip, m_startTransition, m_offset, m_trackOffset);
    m_doIt = true;
}

