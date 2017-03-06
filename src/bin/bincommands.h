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
#include <QDomElement>
#include <QMap>

class Bin;

class AddBinFolderCommand : public QUndoCommand
{
public:
    explicit AddBinFolderCommand(Bin *bin, const QString &id, const QString &name, const QString &parentId, bool remove = false, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
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
    explicit MoveBinClipCommand(Bin *bin, const QString &clipId, const QString &oldParentId, const QString &newParentId, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QString m_clipId;
    QString m_oldParentId;
    QString m_newParentId;
};

class MoveBinFolderCommand : public QUndoCommand
{
public:
    explicit MoveBinFolderCommand(Bin *bin, const QString &clipId, const QString &oldParentId, const QString &newParentId, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QString m_clipId;
    QString m_oldParentId;
    QString m_newParentId;
};

class RenameBinFolderCommand : public QUndoCommand
{
public:
    explicit RenameBinFolderCommand(Bin *bin, const QString &folderId, const QString &newName, const QString &oldName, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QString m_clipId;
    QString m_oldName;
    QString m_newName;
};

class AddBinEffectCommand : public QUndoCommand
{
public:
    explicit AddBinEffectCommand(Bin *bin, const QString &clipId, QDomElement &effect, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QString m_clipId;
    QDomElement m_effect;
};

class RemoveBinEffectCommand : public QUndoCommand
{
public:
    explicit RemoveBinEffectCommand(Bin *bin, const QString &clipId, QDomElement &effect, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QString m_clipId;
    QDomElement m_effect;
};

class UpdateBinEffectCommand : public QUndoCommand
{
public:
    explicit UpdateBinEffectCommand(Bin *bin, const QString &clipId, QDomElement &oldEffect, QDomElement &newEffect, int ix, bool refreshStack, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QString m_clipId;
    QDomElement m_oldEffect;
    QDomElement m_newEffect;
    int m_ix;
    bool m_refreshStack;
};

class ChangeMasterEffectStateCommand : public QUndoCommand
{
public:
    ChangeMasterEffectStateCommand(Bin *bin, const QString &clipId, const QList<int> &effectIndexes, bool disable, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QString m_clipId;
    QList<int> m_effectIndexes;
    bool m_disable;
    bool m_refreshEffectStack;
};

class MoveBinEffectCommand : public QUndoCommand
{
public:
    explicit MoveBinEffectCommand(Bin *bin, const QString &clipId, const QList<int> &oldPos, int newPos, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QString m_clipId;
    QList<int> m_oldindex;
    QList<int> m_newindex;
};

class RenameBinSubClipCommand : public QUndoCommand
{
public:
    explicit RenameBinSubClipCommand(Bin *bin, const QString &clipId, const QString &newName, const QString &oldName, int in, int out, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
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
    explicit AddBinClipCutCommand(Bin *bin, const QString &clipId, int in, int out, bool add, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QString m_clipId;
    int m_in;
    int m_out;
    bool m_addCut;
};

class EditClipCommand : public QUndoCommand
{
public:
    EditClipCommand(Bin *bin, const QString &id, const QMap<QString, QString> &oldparams, const QMap<QString, QString> &newparams, bool doIt, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QMap<QString, QString> m_oldparams;
    QMap<QString, QString> m_newparams;
    QString m_id;
    /** @brief Should this command be executed on first redo ? TODO: we should refactor the code to get rid of this and always execute actions through the command system.
     *. */
    bool m_doIt;
    /** @brief This value is true is this is the first time we execute the command, false otherwise. This allows us to refresh the properties panel
     * only on the later executions of the command, since on the first execution, the properties panel already contains the correct info. */
    bool m_firstExec;
};

class AddClipCommand : public QUndoCommand
{
public:
    AddClipCommand(Bin *bin, const QDomElement &xml, const QString &id, bool doIt, QUndoCommand *parent = nullptr);
    void undo() Q_DECL_OVERRIDE;
    void redo() Q_DECL_OVERRIDE;
private:
    Bin *m_bin;
    QDomElement m_xml;
    QString m_id;
    bool m_doIt;
};

#endif

