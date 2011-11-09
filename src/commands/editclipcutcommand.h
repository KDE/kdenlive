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


#ifndef EDITCLIPCUTCOMMAND_H
#define EDITCLIPCUTCOMMAND_H

#include <QUndoCommand>
#include <QPoint>

#include <KDebug>

class ProjectList;

class EditClipCutCommand : public QUndoCommand
{
public:
    EditClipCutCommand(ProjectList *list, const QString &id, const QPoint oldZone, const QPoint newZone, const QString &oldComment, const QString &newComment, bool doIt, QUndoCommand * parent = 0);

    virtual void undo();
    virtual void redo();

private:
    ProjectList *m_list;
    QString m_id;
    QPoint m_oldZone;
    QPoint m_newZone;
    QString m_oldComment;
    QString m_newComment;
    bool m_doIt;
};

#endif

