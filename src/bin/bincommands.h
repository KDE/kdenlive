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

#include <QMap>
#include <QUndoCommand>

class Bin;

class MoveBinClipCommand : public QUndoCommand
{
public:
    explicit MoveBinClipCommand(Bin *bin, QString clipId, QString oldParentId, QString newParentId, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Bin *m_bin;
    QString m_clipId;
    QString m_oldParentId;
    QString m_newParentId;
};

class MoveBinFolderCommand : public QUndoCommand
{
public:
    explicit MoveBinFolderCommand(Bin *bin, QString clipId, QString oldParentId, QString newParentId, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Bin *m_bin;
    QString m_clipId;
    QString m_oldParentId;
    QString m_newParentId;
};

class RenameBinSubClipCommand : public QUndoCommand
{
public:
    explicit RenameBinSubClipCommand(Bin *bin, QString clipId, QString newName, QString oldName, int in, int out, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Bin *m_bin;
    QString m_clipId;
    QString m_oldName;
    QString m_newName;
    int m_in;
    int m_out;
};

class EditClipCommand : public QUndoCommand
{
public:
    EditClipCommand(Bin *bin, QString id, QMap<QString, QString> oldparams, QMap<QString, QString> newparams, bool doIt, QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Bin *m_bin;
    QMap<QString, QString> m_oldparams;
    QMap<QString, QString> m_newparams;
    QString m_id;
    /** @brief Should this command be executed on first redo ? TODO: we should refactor the code to get rid of this and always execute actions through the
     *command system.
     *. */
    bool m_doIt;
    /** @brief This value is true is this is the first time we execute the command, false otherwise. This allows us to refresh the properties panel
     * only on the later executions of the command, since on the first execution, the properties panel already contains the correct info. */
    bool m_firstExec;
};

#endif
