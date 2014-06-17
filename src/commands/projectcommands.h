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


#ifndef PROJECTCOMMANDS_H
#define PROJECTCOMMANDS_H

#include <QUndoCommand>
#include <QMap>
#include <QPoint>
class ProjectList;

class AddClipCutCommand : public QUndoCommand
{
public:
    AddClipCutCommand(ProjectList *list, const QString &id, int in, int out, const QString &desc, bool newItem, bool remove, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    ProjectList *m_list;
    QString m_id;
    int m_in;
    int m_out;
    QString m_desc;
    bool m_newItem;
    bool m_remove;
};

class AddFolderCommand : public QUndoCommand
{
public:
    AddFolderCommand(ProjectList *view, const QString &folderName, const QString &clipId, bool doIt, QUndoCommand *parent = 0);
    void undo();
    void redo();
private:
    ProjectList *m_view;
    QString m_name;
    QString m_id;
    bool m_doIt;
};

class EditClipCommand : public QUndoCommand
{
public:
    EditClipCommand(ProjectList *list, const QString &id, const QMap <QString, QString> &oldparams, const QMap <QString, QString> &newparams, bool doIt, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    ProjectList *m_list;
    QMap <QString, QString> m_oldparams;
    QMap <QString, QString> m_newparams;
    QString m_id;
    bool m_doIt;
};

class ProjectList;

class EditClipCutCommand : public QUndoCommand
{
public:
    EditClipCutCommand(ProjectList *list, const QString &id, const QPoint &oldZone, const QPoint &newZone, const QString &oldComment, const QString &newComment, bool doIt, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    ProjectList *m_list;
    QString m_id;
    QPoint m_oldZone;
    QPoint m_newZone;
    QString m_oldComment;
    QString m_newComment;
    bool m_doIt;
};

class EditFolderCommand : public QUndoCommand
{
public:
    EditFolderCommand(ProjectList *view, const QString &newfolderName, const QString &oldfolderName, const QString &clipId, bool doIt, QUndoCommand *parent = 0);
    void undo();
    void redo();
private:
    ProjectList *m_view;
    QString m_name;
    QString m_oldname;
    QString m_id;
    bool m_doIt;
};

#endif

