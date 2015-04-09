/***************************************************************************
 *   Copyright (C) 2015 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef BINCOMMANDS_H
#define BINCOMMANDS_H

#include <QUndoCommand>

class Bin;


class AddBinFolderCommand : public QUndoCommand
{
public:
    explicit AddBinFolderCommand(Bin *bin, const QString &id, const QString &name, const QString &parentId, bool remove = false, QUndoCommand * parent = 0);
    void undo();
    void redo();
private:
    Bin *m_bin;
    QString m_id;
    QString m_name;
    QString m_parentId;
    bool m_remove;
};


class MoveBinClipCommand : public QUndoCommand
{
public:
    explicit MoveBinClipCommand(Bin *bin, const QString &clipId, const QString &oldParentId, const QString &newParentId, QUndoCommand *parent = 0);
    void undo();
    void redo();
private:
    Bin *m_bin;
    QString m_clipId;
    QString m_oldParentId;
    QString m_newParentId;
};

class MoveBinFolderCommand : public QUndoCommand
{
public:
    explicit MoveBinFolderCommand(Bin *bin, const QString &clipId, const QString &oldParentId, const QString &newParentId, QUndoCommand *parent = 0);
    void undo();
    void redo();
private:
    Bin *m_bin;
    QString m_clipId;
    QString m_oldParentId;
    QString m_newParentId;
};

class RenameBinFolderCommand : public QUndoCommand
{
public:
    explicit RenameBinFolderCommand(Bin *bin, const QString &folderId, const QString &newName, const QString &oldName, QUndoCommand *parent = 0);
    void undo();
    void redo();
private:
    Bin *m_bin;
    QString m_clipId;
    QString m_oldName;
    QString m_newName;
};

class AddBinEffectCommand : public QUndoCommand
{
public:
    explicit AddBinEffectCommand(Bin *bin, const QString &clipId, const QString &effect, QUndoCommand *parent = 0);
    void undo();
    void redo();
private:
    Bin *m_bin;
    QString m_clipId;
    QString m_effect;
};

class RenameBinSubClipCommand : public QUndoCommand
{
public:
    explicit RenameBinSubClipCommand(Bin *bin, const QString &clipId, const QString &newName, const QString &oldName, int in, int out, QUndoCommand *parent = 0);
    void undo();
    void redo();
private:
    Bin *m_bin;
    QString m_clipId;
    QString m_oldName;
    QString m_newName;
    int m_in;
    int m_out;
};

class AddBinClipCutCommand : public QUndoCommand
{
public:
    explicit AddBinClipCutCommand(Bin *bin, const QString &clipId, int in, int out, bool add, QUndoCommand *parent = 0);
    void undo();
    void redo();
private:
    Bin *m_bin;
    QString m_clipId;
    int m_in;
    int m_out;
    bool m_addCut;
};

#endif

