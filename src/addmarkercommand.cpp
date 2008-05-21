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
#include <KLocale>

#include "addmarkercommand.h"
#include "customtrackview.h"

AddMarkerCommand::AddMarkerCommand(CustomTrackView *view, const QString &oldcomment, const QString &comment, const int id, const GenTime &pos, bool doIt) : m_view(view), m_oldcomment(oldcomment), m_comment(comment), m_id(id), m_pos(pos), m_doIt(doIt) {
    if (m_comment.isEmpty()) setText(i18n("Delete marker"));
    else setText(i18n("Add marker"));
}


// virtual
void AddMarkerCommand::undo() {
    m_view->addMarker(m_id, m_pos, m_oldcomment);
}
// virtual
void AddMarkerCommand::redo() {
    if (m_doIt) {
        m_view->addMarker(m_id, m_pos, m_comment);
    }
    m_doIt = true;
}

#include "addmarkercommand.moc"
