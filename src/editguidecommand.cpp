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

#include "editguidecommand.h"
#include "customtrackview.h"

EditGuideCommand::EditGuideCommand(CustomTrackView *view, const GenTime oldPos, const QString &oldcomment, const GenTime pos, const QString &comment, bool doIt, QUndoCommand * parent) : QUndoCommand(parent), m_view(view), m_oldPos(oldPos), m_oldcomment(oldcomment), m_pos(pos), m_comment(comment), m_doIt(doIt) {
    if (m_oldcomment.isEmpty()) setText(i18n("Add guide"));
    else if (m_oldPos == m_pos) setText(i18n("Edit guide"));
    else if (m_pos <= GenTime()) setText(i18n("Delete guide"));
    else setText(i18n("Move guide"));
}


// virtual
void EditGuideCommand::undo() {
    m_view->editGuide(m_pos, m_oldPos, m_oldcomment);
}
// virtual
void EditGuideCommand::redo() {
    if (m_doIt) {
        m_view->editGuide(m_oldPos, m_pos, m_comment);
    }
    m_doIt = true;
}

#include "editguidecommand.moc"
