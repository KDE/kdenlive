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


#ifndef ADDFOLDERCOMMAND_H
#define ADDFOLDERCOMMAND_H

#include <QUndoCommand>

class ProjectList;

class AddFolderCommand : public QUndoCommand
{
public:
    AddFolderCommand(ProjectList *view, const QString folderName, const QString &clipId, bool doIt, QUndoCommand *parent = 0);

    virtual void undo();
    virtual void redo();

private:
    ProjectList *m_view;
    QString m_name;
    QString m_id;
    bool m_doIt;
};

#endif

