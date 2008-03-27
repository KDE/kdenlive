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

#include "addtransitioncommand.h"
#include "customtrackview.h"

AddTransitionCommand::AddTransitionCommand(CustomTrackView *view, ItemInfo info, int transitiontrack, QDomElement params, bool doIt) : m_view(view), m_info(info), m_track(transitiontrack), m_params(params), m_doIt(doIt) {
    if (m_doIt) setText(i18n("Add transition to clip"));
    else setText(i18n("Delete transition from clip"));
}


// virtual
void AddTransitionCommand::undo() {
    if (m_doIt) m_view->deleteTransition(m_info, m_track, m_params);
    else m_view->addTransition(m_info, m_track, m_params);
}
// virtual
void AddTransitionCommand::redo() {
    if (m_doIt) m_view->addTransition(m_info, m_track, m_params);
    else m_view->deleteTransition(m_info, m_track, m_params);
    m_doIt = true;
}

#include "addtimelineclipcommand.moc"
