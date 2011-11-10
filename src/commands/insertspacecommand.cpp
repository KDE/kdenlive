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


#include "insertspacecommand.h"
#include "customtrackview.h"

#include <KLocale>

InsertSpaceCommand::InsertSpaceCommand(CustomTrackView *view, QList<ItemInfo> clipsToMove, QList<ItemInfo> transToMove, int track, const GenTime &duration, bool doIt, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_clipsToMove(clipsToMove),
        m_transToMove(transToMove),
        m_duration(duration),
        m_track(track),
        m_doIt(doIt)
{
    if (duration > GenTime()) setText(i18n("Insert space"));
    else setText(i18n("Remove space"));
}

// virtual
void InsertSpaceCommand::undo()
{
    // kDebug()<<"----  undoing action";
    m_view->insertSpace(m_clipsToMove, m_transToMove, m_track, GenTime() - m_duration, m_duration);
}
// virtual
void InsertSpaceCommand::redo()
{
    // kDebug() << "----  redoing action cut: " << m_cutTime.frames(25);
    if (m_doIt) {
        m_view->insertSpace(m_clipsToMove, m_transToMove, m_track, m_duration, GenTime());
    }
    m_doIt = true;
}

