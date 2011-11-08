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


#include "groupclipscommand.h"
#include "customtrackview.h"

#include <KLocale>

GroupClipsCommand::GroupClipsCommand(CustomTrackView *view, const QList <ItemInfo> clipInfos, const QList <ItemInfo> transitionInfos, bool group, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_clips(clipInfos),
        m_transitions(transitionInfos),
        m_group(group)
{
    if (m_group) setText(i18n("Group clips"));
    else setText(i18n("Ungroup clips"));
}


// virtual
void GroupClipsCommand::undo()
{
// kDebug()<<"----  undoing action";
    m_view->doGroupClips(m_clips, m_transitions, !m_group);
}
// virtual
void GroupClipsCommand::redo()
{
    kDebug() << "----  redoing action";
    m_view->doGroupClips(m_clips, m_transitions, m_group);
}

