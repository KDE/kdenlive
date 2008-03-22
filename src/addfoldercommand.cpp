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

#include "addfoldercommand.h"
#include "kdenlivedoc.h"

AddFolderCommand::AddFolderCommand(KdenliveDoc *doc, const QString folderName, int clipId, bool doIt)
        : m_doc(doc), m_name(folderName), m_id(clipId), m_doIt(doIt) {
    if (doIt) setText(i18n("Add folder"));
    else setText(i18n("Delete folder"));
}

// virtual
void AddFolderCommand::undo() {
    if (m_doIt) m_doc->deleteFolder(m_name, m_id);
    else m_doc->addFolder(m_name, m_id, false);
}
// virtual
void AddFolderCommand::redo() {
    if (m_doIt) m_doc->addFolder(m_name, m_id, false);
    else m_doc->deleteFolder(m_name, m_id);
}

#include "addfoldercommand.moc"
