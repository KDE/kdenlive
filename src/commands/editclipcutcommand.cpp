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

#include "editclipcutcommand.h"
#include "projectlist.h"

#include <KLocale>

EditClipCutCommand::EditClipCutCommand(ProjectList *list, const QString &id, const QPoint oldZone, const QPoint newZone, const QString &oldComment, const QString &newComment, bool doIt, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_list(list),
        m_id(id),
        m_oldZone(oldZone),
        m_newZone(newZone),
        m_oldComment(oldComment),
        m_newComment(newComment),
        m_doIt(doIt)
{
    setText(i18n("Edit clip cut"));
}


// virtual
void EditClipCutCommand::undo()
{
    kDebug() << "----  undoing action";
    m_list->doUpdateClipCut(m_id, m_newZone, m_oldZone, m_oldComment);
}
// virtual
void EditClipCutCommand::redo()
{
    kDebug() << "----  redoing action";
    if (m_doIt) m_list->doUpdateClipCut(m_id, m_oldZone, m_newZone, m_newComment);
    m_doIt = true;
}

