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


#include "changeeffectstatecommand.h"
#include "customtrackview.h"

#include <KLocale>

ChangeEffectStateCommand::ChangeEffectStateCommand(CustomTrackView *view, const int track, const GenTime& pos, const QList <int>& effectIndexes, bool disable, bool refreshEffectStack, bool doIt, QUndoCommand *parent) :
        QUndoCommand(parent),
        m_view(view),
        m_track(track),
        m_effectIndexes(effectIndexes),
        m_pos(pos),
        m_disable(disable),
        m_doIt(doIt),
        m_refreshEffectStack(refreshEffectStack)
{
    if (disable) 
	setText(i18np("Disable effect", "Disable effects", effectIndexes.count()));
    else
	setText(i18np("Enable effect", "Enable effects", effectIndexes.count()));
}

// virtual
void ChangeEffectStateCommand::undo()
{
    m_view->updateEffectState(m_track, m_pos, m_effectIndexes, !m_disable, true);
}
// virtual
void ChangeEffectStateCommand::redo()
{
    if (m_doIt) m_view->updateEffectState(m_track, m_pos, m_effectIndexes, m_disable, m_refreshEffectStack);
    m_doIt = true;
    m_refreshEffectStack = true;
}


