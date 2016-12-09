/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2014 by Vincent Pinon (vpinon@april.org)                *
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

#include "projectcommands.h"
#include "bin/projectclip.h"
#include "doc/kdenlivedoc.h"

#include <klocalizedstring.h>

AddClipCutCommand::AddClipCutCommand(ProjectList *list, const QString &id, int in, int out, const QString &desc, bool newItem, bool remove, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_list(list),
    m_id(id),
    m_in(in),
    m_out(out),
    m_desc(desc),
    m_newItem(newItem),
    m_remove(remove)
{
    setText(i18n("Add clip cut"));
}
// virtual
void AddClipCutCommand::undo()
{
    Q_UNUSED(m_list)
    Q_UNUSED(m_id)
    Q_UNUSED(m_in)
    Q_UNUSED(m_out)
    Q_UNUSED(m_desc)
    Q_UNUSED(m_newItem)
    Q_UNUSED(m_remove)
    /*if (m_remove)
        m_list->addClipCut(m_id, m_in, m_out, m_desc, m_newItem);
    else
        m_list->removeClipCut(m_id, m_in, m_out);*/
}
// virtual
void AddClipCutCommand::redo()
{
    /*if (m_remove)
        m_list->removeClipCut(m_id, m_in, m_out);
    else
        m_list->addClipCut(m_id, m_in, m_out, m_desc, m_newItem);*/
}

AddFolderCommand::AddFolderCommand(ProjectList *view, const QString &folderName, const QString &clipId, bool doIt, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_view(view),
    m_name(folderName),
    m_id(clipId),
    m_doIt(doIt)
{
    if (doIt) {
        setText(i18n("Add folder"));
    } else {
        setText(i18n("Delete folder"));
    }
}
// virtual
void AddFolderCommand::undo()
{
    Q_UNUSED(m_view)
    Q_UNUSED(m_name)
    Q_UNUSED(m_id)
    Q_UNUSED(m_doIt)
    /*if (m_doIt)
        m_view->slotAddFolder(m_name, m_id, true);
    else
        m_view->slotAddFolder(m_name, m_id, false);*/
}
// virtual
void AddFolderCommand::redo()
{
    /*if (m_doIt)
        m_view->slotAddFolder(m_name, m_id, false);
    else
        m_view->slotAddFolder(m_name, m_id, true);*/
}

EditClipCutCommand::EditClipCutCommand(ProjectList *list, const QString &id, const QPoint &oldZone, const QPoint &newZone, const QString &oldComment, const QString &newComment, bool doIt, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_list(list),
    m_id(id),
    m_oldZone(oldZone),
    m_newZone(newZone),
    m_oldComment(oldComment),
    m_newComment(newComment),
    m_doIt(doIt)
{
    setText(i18n("Edit clip cut"));
}
// virtual
void EditClipCutCommand::undo()
{
    Q_UNUSED(m_list)
    Q_UNUSED(m_id)
    Q_UNUSED(m_oldZone)
    Q_UNUSED(m_newZone)
    Q_UNUSED(m_oldComment)
    Q_UNUSED(m_newComment)
    Q_UNUSED(m_doIt)
    //m_list->doUpdateClipCut(m_id, m_newZone, m_oldZone, m_oldComment);
}
// virtual
void EditClipCutCommand::redo()
{
    /*if (m_doIt)
        m_list->doUpdateClipCut(m_id, m_oldZone, m_newZone, m_newComment); */
}

EditFolderCommand::EditFolderCommand(ProjectList *view, const QString &newfolderName, const QString &oldfolderName, const QString &clipId, bool doIt, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_view(view),
    m_name(newfolderName),
    m_oldname(oldfolderName),
    m_id(clipId),
    m_doIt(doIt)
{
    setText(i18n("Rename folder"));
}
// virtual
void EditFolderCommand::undo()
{
    Q_UNUSED(m_view)
    Q_UNUSED(m_name)
    Q_UNUSED(m_oldname)
    Q_UNUSED(m_id)
    Q_UNUSED(m_doIt)
    //m_view->slotAddFolder(m_oldname, m_id, false, true);
}
// virtual
void EditFolderCommand::redo()
{
    //if (m_doIt) m_view->slotAddFolder(m_name, m_id, false, true);
}

AddMarkerCommand::AddMarkerCommand(ProjectClip *clip, QList<CommentedTime> &oldMarkers, QList<CommentedTime> &newMarkers, QUndoCommand *parent) :
    QUndoCommand(parent),
    m_clip(clip),
    m_oldMarkers(oldMarkers),
    m_newMarkers(newMarkers)
{
    if (m_newMarkers.isEmpty()) {
        return;
    }
    if (m_newMarkers.first().markerType() < 0) {
        setText(i18n("Delete marker"));
    } else if (m_oldMarkers.first().comment().isEmpty()) {
        setText(i18n("Add marker"));
    } else {
        setText(i18n("Edit marker"));
    }
}
// virtual
void AddMarkerCommand::undo()
{
    m_clip->addMarkers(m_oldMarkers);
}
// virtual
void AddMarkerCommand::redo()
{
    m_clip->addMarkers(m_newMarkers);
}
