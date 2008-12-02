/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include <KLocale>

#include "addtrackcommand.h"
#include "customtrackview.h"

AddTrackCommand::AddTrackCommand(CustomTrackView *view, int ix, TrackInfo info, bool addTrack, bool doIt, QUndoCommand * parent) : QUndoCommand(parent), m_view(view), m_ix(ix), m_info(info), m_addTrack(addTrack), m_doIt(doIt) {
    if (addTrack) setText(i18n("Add track"));
    else setText(i18n("Delete track"));
}


// virtual
void AddTrackCommand::undo() {
// kDebug()<<"----  undoing action";
    m_doIt = true;
    if (m_addTrack) m_view->deleteTimelineTrack(m_ix);
    else m_view->addTimelineTrack(m_ix, m_info);
}
// virtual
void AddTrackCommand::redo() {
    kDebug() << "----  redoing action";
    if (m_doIt) {
        if (m_addTrack) m_view->addTimelineTrack(m_ix, m_info);
        else m_view->deleteTimelineTrack(m_ix);
    }
    m_doIt = true;
}

