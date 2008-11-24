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

#include "insertspacecommand.h"
#include "customtrackview.h"

InsertSpaceCommand::InsertSpaceCommand(CustomTrackView *view, const GenTime &pos, int track, const GenTime &duration, bool doIt, QUndoCommand * parent) : QUndoCommand(parent), m_view(view), m_pos(pos), m_track(track), m_duration(duration), m_doIt(doIt) {
    setText(i18n("Insert space"));
}

// virtual
void InsertSpaceCommand::undo() {
    // kDebug()<<"----  undoing action";
    m_view->insertSpace(m_pos, m_track, m_duration, false);
}
// virtual
void InsertSpaceCommand::redo() {
    // kDebug() << "----  redoing action cut: " << m_cutTime.frames(25);
    if (m_doIt)
        m_view->insertSpace(m_pos, m_track, m_duration, true);
    m_doIt = true;
}

