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


#include "razorgroupcommand.h"
#include "customtrackview.h"

RazorGroupCommand::RazorGroupCommand(CustomTrackView *view, QList <ItemInfo> clips1, QList <ItemInfo> transitions1, QList <ItemInfo> clipsCut, QList <ItemInfo> transitionsCut, QList <ItemInfo> clips2, QList <ItemInfo> transitions2, GenTime cutPos, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_clips1(clips1),
    m_transitions1(transitions1),
    m_clipsCut(clipsCut),
    m_transitionsCut(transitionsCut),
    m_clips2(clips2),
    m_transitions2(transitions2),
    m_cutPos(cutPos)
{
    setText(i18n("Cut Group"));
}

// virtual
void RazorGroupCommand::undo()
{
    m_view->slotRazorGroup(m_clips1, m_transitions1, m_clipsCut, m_transitionsCut, m_clips2, m_transitions2, m_cutPos, false);
}

// virtual
void RazorGroupCommand::redo()
{
    m_view->slotRazorGroup(m_clips1, m_transitions1, m_clipsCut, m_transitionsCut, m_clips2, m_transitions2, m_cutPos, true);
}
