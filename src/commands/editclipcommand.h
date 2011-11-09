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


#ifndef EDITCLIPCOMMAND_H
#define EDITCLIPCOMMAND_H

#include <QUndoCommand>
#include <KDebug>
#include <QDomElement>

class ProjectList;

class EditClipCommand : public QUndoCommand
{
public:
    EditClipCommand(ProjectList *list, const QString &id, QMap <QString, QString> oldparams, QMap <QString, QString> newparams, bool doIt, QUndoCommand * parent = 0);

    virtual void undo();
    virtual void redo();

private:
    ProjectList *m_list;
    QMap <QString, QString> m_oldparams;
    QMap <QString, QString> m_newparams;
    QString m_id;
    bool m_doIt;
};

#endif

