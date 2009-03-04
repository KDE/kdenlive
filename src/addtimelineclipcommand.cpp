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


#include "addtimelineclipcommand.h"
#include "customtrackview.h"

#include <KLocale>

AddTimelineClipCommand::AddTimelineClipCommand(CustomTrackView *view, QDomElement xml, const QString &clipId, ItemInfo info, EffectsList effects, bool doIt, bool doRemove, QUndoCommand * parent) : QUndoCommand(parent), m_view(view), m_xml(xml), m_clipId(clipId), m_clipInfo(info), m_effects(effects),  m_doIt(doIt), m_remove(doRemove) {
    if (!m_remove) setText(i18n("Add timeline clip"));
    else setText(i18n("Delete timeline clip"));
}


// virtual
void AddTimelineClipCommand::undo() {
    if (!m_remove) m_view->deleteClip(m_clipInfo);
    else m_view->addClip(m_xml, m_clipId, m_clipInfo, m_effects);
}
// virtual
void AddTimelineClipCommand::redo() {
    if (m_doIt) {
        if (!m_remove) m_view->addClip(m_xml, m_clipId, m_clipInfo, m_effects);
        else m_view->deleteClip(m_clipInfo);
    }
    m_doIt = true;
}


