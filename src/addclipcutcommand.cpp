/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "addclipcutcommand.h"
#include "projectlist.h"

#include <KLocale>

AddClipCutCommand::AddClipCutCommand(ProjectList *list, const QString &id, int in, int out, const QString desc, bool newItem, bool remove, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_list(list),
        m_id(id),
        m_in(in),
        m_out(out),
        m_desc(desc),
        m_newItem(newItem),
        m_remove(remove)
{
    setText(i18n("Add clip cut"));
}


// virtual
void AddClipCutCommand::undo()
{
    if (m_remove) m_list->addClipCut(m_id, m_in, m_out, m_desc, m_newItem);
    else m_list->removeClipCut(m_id, m_in, m_out);
}
// virtual
void AddClipCutCommand::redo()
{
    if (m_remove) m_list->removeClipCut(m_id, m_in, m_out);
    else m_list->addClipCut(m_id, m_in, m_out, m_desc, m_newItem);
}

