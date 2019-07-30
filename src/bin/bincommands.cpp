/***************************************************************************
 *   Copyright (C) 2015 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
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

#include "bincommands.h"
#include "bin.h"

#include <klocalizedstring.h>
#include <utility>
MoveBinClipCommand::MoveBinClipCommand(Bin *bin, QString clipId, QString oldParentId, QString newParentId, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_bin(bin)
    , m_clipId(std::move(clipId))
    , m_oldParentId(std::move(oldParentId))
    , m_newParentId(std::move(newParentId))
{
    setText(i18n("Move Clip"));
}
// virtual
void MoveBinClipCommand::undo()
{
    m_bin->doMoveClip(m_clipId, m_oldParentId);
}
// virtual
void MoveBinClipCommand::redo()
{
    m_bin->doMoveClip(m_clipId, m_newParentId);
}

MoveBinFolderCommand::MoveBinFolderCommand(Bin *bin, QString clipId, QString oldParentId, QString newParentId, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_bin(bin)
    , m_clipId(std::move(clipId))
    , m_oldParentId(std::move(oldParentId))
    , m_newParentId(std::move(newParentId))
{
    setText(i18n("Move Clip"));
}
// virtual
void MoveBinFolderCommand::undo()
{
    m_bin->doMoveFolder(m_clipId, m_oldParentId);
}
// virtual
void MoveBinFolderCommand::redo()
{
    m_bin->doMoveFolder(m_clipId, m_newParentId);
}

RenameBinSubClipCommand::RenameBinSubClipCommand(Bin *bin, QString clipId, QString newName, QString oldName, int in, int out, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_bin(bin)
    , m_clipId(std::move(clipId))
    , m_oldName(std::move(oldName))
    , m_newName(std::move(newName))
    , m_in(in)
    , m_out(out)
{
    setText(i18n("Rename Zone"));
}
// virtual
void RenameBinSubClipCommand::undo()
{
    m_bin->renameSubClip(m_clipId, m_oldName, m_in, m_out);
}
// virtual
void RenameBinSubClipCommand::redo()
{
    m_bin->renameSubClip(m_clipId, m_newName, m_in, m_out);
}

EditClipCommand::EditClipCommand(Bin *bin, QString id, QMap<QString, QString> oldparams, QMap<QString, QString> newparams, bool doIt, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_bin(bin)
    , m_oldparams(std::move(oldparams))
    , m_newparams(std::move(newparams))
    , m_id(std::move(id))
    , m_doIt(doIt)
    , m_firstExec(true)
{
    setText(i18n("Edit clip"));
}
// virtual
void EditClipCommand::undo()
{
    m_bin->slotUpdateClipProperties(m_id, m_oldparams, true);
}
// virtual
void EditClipCommand::redo()
{
    if (m_doIt) {
        m_bin->slotUpdateClipProperties(m_id, m_newparams, !m_firstExec);
    }
    m_doIt = true;
    m_firstExec = false;
}
