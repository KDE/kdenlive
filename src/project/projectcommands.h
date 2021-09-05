/*
 *   SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 ***************************************************************************/

#ifndef PROJECTCOMMANDS_H
#define PROJECTCOMMANDS_H

#include "definitions.h"

#include <QPoint>
#include <QUndoCommand>

class ProjectList;

class AddClipCutCommand : public QUndoCommand
{
public:
    AddClipCutCommand(ProjectList *list, QString id, int in, int out, QString desc, bool newItem, bool remove, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

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
    AddFolderCommand(ProjectList *view, QString folderName, QString clipId, bool doIt, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ProjectList *m_view;
    QString m_name;
    QString m_id;
    bool m_doIt;
};

class EditClipCutCommand : public QUndoCommand
{
public:
    EditClipCutCommand(ProjectList *list, QString id, const QPoint &oldZone, const QPoint &newZone, QString oldComment, QString newComment, bool doIt,
                       QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

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
    EditFolderCommand(ProjectList *view, QString newfolderName, QString oldfolderName, QString clipId, bool doIt, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    ProjectList *m_view;
    QString m_name;
    QString m_oldname;
    QString m_id;
    bool m_doIt;
};

#endif
