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

AddBinFolderCommand::AddBinFolderCommand(Bin *bin, const QString &id, const QString &name, const QString &parentId, bool remove, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_id(id),
    m_name(name),
    m_parentId(parentId),
    m_remove(remove)
{
    if (remove) {
        setText(i18n("Remove Folder"));
    } else {
        setText(i18n("Add Folder"));
    }
}
// virtual
void AddBinFolderCommand::undo()
{
    if (m_remove) {
        m_bin->doAddFolder(m_id, m_name, m_parentId);
    } else {
        m_bin->doRemoveFolder(m_id);
    }
}
// virtual
void AddBinFolderCommand::redo()
{
    if (m_remove) {
        m_bin->doRemoveFolder(m_id);
    } else {
        m_bin->doAddFolder(m_id, m_name, m_parentId);
    }
}

MoveBinClipCommand::MoveBinClipCommand(Bin *bin, const QString &clipId, const QString &oldParentId, const QString &newParentId, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_clipId(clipId),
    m_oldParentId(oldParentId),
    m_newParentId(newParentId)
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

MoveBinFolderCommand::MoveBinFolderCommand(Bin *bin, const QString &clipId, const QString &oldParentId, const QString &newParentId, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_clipId(clipId),
    m_oldParentId(oldParentId),
    m_newParentId(newParentId)
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

RenameBinFolderCommand::RenameBinFolderCommand(Bin *bin, const QString &folderId, const QString &newName, const QString &oldName, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_clipId(folderId),
    m_oldName(oldName),
    m_newName(newName)
{
    setText(i18n("Rename Folder"));
}
// virtual
void RenameBinFolderCommand::undo()
{
    m_bin->renameFolder(m_clipId, m_oldName);
}
// virtual
void RenameBinFolderCommand::redo()
{
    m_bin->renameFolder(m_clipId, m_newName);
}

AddBinEffectCommand::AddBinEffectCommand(Bin *bin, const QString &clipId, QDomElement &effect, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_clipId(clipId),
    m_effect(effect)
{
    setText(i18n("Add Bin Effect"));
}
// virtual
void AddBinEffectCommand::undo()
{
    m_bin->removeEffect(m_clipId, m_effect);
}
// virtual
void AddBinEffectCommand::redo()
{
    m_bin->addEffect(m_clipId, m_effect);
}

RemoveBinEffectCommand::RemoveBinEffectCommand(Bin *bin, const QString &clipId, QDomElement &effect, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_clipId(clipId),
    m_effect(effect)
{
    setText(i18n("Remove Bin Effect"));
}
// virtual
void RemoveBinEffectCommand::undo()
{
    m_bin->addEffect(m_clipId, m_effect);
}
// virtual
void RemoveBinEffectCommand::redo()
{
    m_bin->removeEffect(m_clipId, m_effect);
}

UpdateBinEffectCommand::UpdateBinEffectCommand(Bin *bin, const QString &clipId, QDomElement &oldEffect,  QDomElement &newEffect, int ix, bool refreshStack, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_clipId(clipId),
    m_oldEffect(oldEffect),
    m_newEffect(newEffect),
    m_ix(ix),
    m_refreshStack(refreshStack)
{
    setText(i18n("Edit Bin Effect"));
}
// virtual
void UpdateBinEffectCommand::undo()
{
    m_bin->updateEffect(m_clipId, m_oldEffect, m_ix, m_refreshStack);
}
// virtual
void UpdateBinEffectCommand::redo()
{
    m_bin->updateEffect(m_clipId, m_newEffect, m_ix, m_refreshStack);
    m_refreshStack = true;
}

ChangeMasterEffectStateCommand::ChangeMasterEffectStateCommand(Bin *bin, const QString &clipId, const QList<int> &effectIndexes, bool disable, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_clipId(clipId),
    m_effectIndexes(effectIndexes),
    m_disable(disable),
    m_refreshEffectStack(false)
{
    if (disable) {
        setText(i18np("Disable effect", "Disable effects", effectIndexes.count()));
    } else {
        setText(i18np("Enable effect", "Enable effects", effectIndexes.count()));
    }
}

// virtual
void ChangeMasterEffectStateCommand::undo()
{
    m_bin->changeEffectState(m_clipId, m_effectIndexes, !m_disable, m_refreshEffectStack);
}
// virtual
void ChangeMasterEffectStateCommand::redo()
{
    m_bin->changeEffectState(m_clipId, m_effectIndexes, m_disable, m_refreshEffectStack);
    m_refreshEffectStack = true;
}

MoveBinEffectCommand::MoveBinEffectCommand(Bin *bin, const QString &clipId, const QList<int> &oldPos, int newPos, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_clipId(clipId),
    m_oldindex(oldPos)
{
    m_newindex.reserve(m_oldindex.count());
    for (int i = 0; i < m_oldindex.count(); ++i) {
        m_newindex << newPos + i;
    }
    setText(i18n("Move Bin Effect"));
}

// virtual
void MoveBinEffectCommand::undo()
{
    m_bin->moveEffect(m_clipId, m_newindex, m_oldindex);
}
// virtual
void MoveBinEffectCommand::redo()
{
    m_bin->moveEffect(m_clipId, m_oldindex, m_newindex);
}

RenameBinSubClipCommand::RenameBinSubClipCommand(Bin *bin, const QString &clipId, const QString &newName, const QString &oldName, int in, int out, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_clipId(clipId),
    m_oldName(oldName),
    m_newName(newName),
    m_in(in),
    m_out(out)
{
    setText(i18n("Rename Zone"));
}
// virtual
void RenameBinSubClipCommand::undo()
{
    m_bin->renameSubClip(m_clipId, m_oldName, m_newName, m_in, m_out);
}
// virtual
void RenameBinSubClipCommand::redo()
{
    m_bin->renameSubClip(m_clipId, m_newName, m_oldName, m_in, m_out);
}

AddBinClipCutCommand::AddBinClipCutCommand(Bin *bin, const QString &clipId, int in, int out, bool add, QUndoCommand *parent) :
    QUndoCommand(parent)
    , m_bin(bin)
    , m_clipId(clipId)
    , m_in(in)
    , m_out(out)
    , m_addCut(add)
{
    setText(i18n("Add Sub Clip"));
}

// virtual
void AddBinClipCutCommand::undo()
{
    if (m_addCut) {
        m_bin->removeClipCut(m_clipId, m_in, m_out);
    } else {
        m_bin->addClipCut(m_clipId, m_in, m_out);
    }
}
// virtual
void AddBinClipCutCommand::redo()
{
    if (m_addCut) {
        m_bin->addClipCut(m_clipId, m_in, m_out);
    } else {
        m_bin->removeClipCut(m_clipId, m_in, m_out);
    }
}

EditClipCommand::EditClipCommand(Bin *bin, const QString &id, const QMap<QString, QString> &oldparams, const QMap<QString, QString> &newparams, bool doIt, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_oldparams(oldparams),
    m_newparams(newparams),
    m_id(id),
    m_doIt(doIt),
    m_firstExec(true)
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

AddClipCommand::AddClipCommand(Bin *bin, const QDomElement &xml, const QString &id, bool doIt, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_bin(bin),
    m_xml(xml),
    m_id(id),
    m_doIt(doIt)
{
    if (doIt) {
        setText(i18n("Add clip"));
    } else {
        setText(i18n("Delete clip"));
    }
}
// virtual
void AddClipCommand::undo()
{
    if (m_doIt) {
        m_bin->deleteClip(m_id);
    } else {
        m_bin->addClip(m_xml, m_id);
    }
}
// virtual
void AddClipCommand::redo()
{
    if (m_doIt) {
        m_bin->addClip(m_xml, m_id);
    } else {
        m_bin->deleteClip(m_id);
    }
}
