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


#ifndef ADDCLIPCUTCOMMAND_H
#define ADDCLIPCUTCOMMAND_H

#include <QUndoCommand>
#include <KDebug>

class ProjectList;

class AddClipCutCommand : public QUndoCommand
{
public:
    AddClipCutCommand(ProjectList *list, const QString &id, int in, int out, const QString desc, bool remove, QUndoCommand * parent = 0);

    virtual void undo();
    virtual void redo();

private:
    ProjectList *m_list;
    QString m_id;
    int m_in;
    int m_out;
    QString m_desc;
    bool m_remove;
};

#endif

