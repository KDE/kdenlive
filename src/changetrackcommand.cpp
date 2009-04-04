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


#include "changetrackcommand.h"
#include "customtrackview.h"

#include <KLocale>

ChangeTrackCommand::ChangeTrackCommand(CustomTrackView *view, int ix, TrackInfo oldInfo, TrackInfo newInfo, bool doIt, QUndoCommand * parent) : QUndoCommand(parent), m_view(view), m_ix(ix), m_oldinfo(oldInfo), m_newinfo(newInfo), m_doIt(doIt)
{
    setText(i18n("Change track type"));
}


// virtual
void ChangeTrackCommand::undo()
{
// kDebug()<<"----  undoing action";
    m_doIt = true;
    m_view->changeTrack(m_ix, m_oldinfo);
}
// virtual
void ChangeTrackCommand::redo()
{
    if (m_doIt) m_view->changeTrack(m_ix, m_newinfo);
    m_doIt = true;
}

