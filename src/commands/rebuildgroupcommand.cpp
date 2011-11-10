/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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


#include "rebuildgroupcommand.h"
#include "customtrackview.h"

RebuildGroupCommand::RebuildGroupCommand(CustomTrackView* view, int childTrack, GenTime childPos, QUndoCommand* parent) :
        QUndoCommand(parent),
        m_view(view),
        m_childTrack(childTrack),
        m_childPos(childPos)
{
    setText(i18n("Rebuild Group"));
}

// virtual
void RebuildGroupCommand::undo()
{
    m_view->rebuildGroup(m_childTrack, m_childPos);
}

// virtual
void RebuildGroupCommand::redo()
{
    m_view->rebuildGroup(m_childTrack, m_childPos);
}
