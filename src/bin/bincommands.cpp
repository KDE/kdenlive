/*
    SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>


SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "bincommands.h"
#include "bin.h"

#include <KLocalizedString>
#include <utility>
MoveBinClipCommand::MoveBinClipCommand(Bin *bin, QMap<QString, std::pair<QString, QString>> clipIds, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_bin(bin)
    , m_clipIds(std::move(clipIds))
{
    setText(QStringLiteral("%1 %2").arg(QTime::currentTime().toString("hh:mm")).arg(i18nc("@action", "Move Clip")));
}
// virtual
void MoveBinClipCommand::undo()
{
    m_bin->doMoveClips(m_clipIds, false);
}
// virtual
void MoveBinClipCommand::redo()
{
    m_bin->doMoveClips(m_clipIds, true);
}

MoveBinFolderCommand::MoveBinFolderCommand(Bin *bin, QString clipId, QString oldParentId, QString newParentId, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_bin(bin)
    , m_clipId(std::move(clipId))
    , m_oldParentId(std::move(oldParentId))
    , m_newParentId(std::move(newParentId))
{
    setText(QStringLiteral("%1 %2").arg(QTime::currentTime().toString("hh:mm")).arg(i18nc("@action", "Move Clip")));
}
// virtual
void MoveBinFolderCommand::undo()
{
    m_bin->doMoveFolder(m_clipId, m_oldParentId);
    QUndoCommand::undo();
}
// virtual
void MoveBinFolderCommand::redo()
{
    m_bin->doMoveFolder(m_clipId, m_newParentId);
    QUndoCommand::redo();
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
    setText(QStringLiteral("%1 %2").arg(QTime::currentTime().toString("hh:mm")).arg(i18nc("@action", "Rename Zone")));
}
// virtual
void RenameBinSubClipCommand::undo()
{
    m_bin->renameSubClip(m_clipId, m_oldName, m_in, m_out);
    QUndoCommand::undo();
}
// virtual
void RenameBinSubClipCommand::redo()
{
    m_bin->renameSubClip(m_clipId, m_newName, m_in, m_out);
    QUndoCommand::redo();
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
    setText(QStringLiteral("%1 %2").arg(QTime::currentTime().toString("hh:mm")).arg(i18nc("@action", "Edit Clip")));
}
// virtual
void EditClipCommand::undo()
{
    m_bin->slotUpdateClipProperties(m_id, m_oldparams, true);
    QUndoCommand::undo();
}
// virtual
void EditClipCommand::redo()
{
    if (m_doIt) {
        m_bin->slotUpdateClipProperties(m_id, m_newparams, !m_firstExec);
    }
    m_doIt = true;
    m_firstExec = false;
    QUndoCommand::redo();
}
