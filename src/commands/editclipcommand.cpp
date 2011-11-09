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

#include "editclipcommand.h"
#include "projectlist.h"

#include <KLocale>

EditClipCommand::EditClipCommand(ProjectList *list, const QString &id, QMap <QString, QString> oldparams, QMap <QString, QString> newparams, bool doIt, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_list(list),
        m_oldparams(oldparams),
        m_newparams(newparams),
        m_id(id),
        m_doIt(doIt)
{
    setText(i18n("Edit clip"));
}


// virtual
void EditClipCommand::undo()
{
    kDebug() << "----  undoing action";
    m_list->slotUpdateClipProperties(m_id, m_oldparams);
}
// virtual
void EditClipCommand::redo()
{
    kDebug() << "----  redoing action";
    if (m_doIt) m_list->slotUpdateClipProperties(m_id, m_newparams);
    m_doIt = true;
}

