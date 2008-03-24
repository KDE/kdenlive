/***************************************************************************
                          movetransitioncommand.h  -  description
                             -------------------
    begin                : Mar 15 2008
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

#include "movetransitioncommand.h"
#include "customtrackview.h"
MoveTransitionCommand::MoveTransitionCommand(CustomTrackView *view, const QPointF startPos, const QPointF endPos, int startTrack, int endTrack, bool doIt)
        : m_view(view), m_startPos(startPos), m_endPos(endPos), m_doIt(doIt) {
    setText(i18n("Move transition"));
}


// virtual
void MoveTransitionCommand::undo() {
// kDebug()<<"----  undoing action";
    m_doIt = true;
    //if (m_doIt) m_view->moveTransition(m_endPos, m_startPos);
}
// virtual
void MoveTransitionCommand::redo() {
    kDebug() << "----  redoing action";
    //if (m_doIt) m_view->moveTransition(m_startPos, m_endPos);
    m_doIt = true;
}

#include "moveclipcommand.moc"
