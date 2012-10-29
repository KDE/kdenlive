/***************************************************************************
 *   Copyright (C) 2012 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include "refreshmonitorcommand.h"
#include "customtrackview.h"


RefreshMonitorCommand::RefreshMonitorCommand(CustomTrackView *view, bool execute, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_exec(execute)
{
}


// virtual
void RefreshMonitorCommand::undo()
{
    m_view->monitorRefresh();
}
// virtual
void RefreshMonitorCommand::redo()
{
    if (m_exec)
	m_view->monitorRefresh();
    m_exec = true;
}


