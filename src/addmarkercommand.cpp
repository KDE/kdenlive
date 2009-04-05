/***************************************************************************
                          addtransitioncommand.cpp  -  description
                             -------------------
    begin                : 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "addmarkercommand.h"
#include "customtrackview.h"

#include <KLocale>

AddMarkerCommand::AddMarkerCommand(CustomTrackView *view, const QString &oldcomment, const QString &comment, const QString &id, const GenTime &pos, QUndoCommand * parent) : QUndoCommand(parent), m_view(view), m_oldcomment(oldcomment), m_comment(comment), m_id(id), m_pos(pos)
{
    if (m_comment.isEmpty()) setText(i18n("Delete marker"));
    else if (m_oldcomment.isEmpty()) setText(i18n("Add marker"));
    else setText(i18n("Edit marker"));
}


// virtual
void AddMarkerCommand::undo()
{
    m_view->addMarker(m_id, m_pos, m_oldcomment);
}
// virtual
void AddMarkerCommand::redo()
{
        m_view->addMarker(m_id, m_pos, m_comment);
}

