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


#include "editeffectcommand.h"
#include "customtrackview.h"

#include <KLocale>

EditEffectCommand::EditEffectCommand(CustomTrackView *view, const int track, GenTime pos, QDomElement oldeffect, QDomElement effect, int stackPos, bool doIt, QUndoCommand *parent) :
        QUndoCommand(parent),
        m_view(view),
        m_track(track),
        m_oldeffect(oldeffect),
        m_effect(effect),
        m_pos(pos),
        m_stackPos(stackPos),
        m_doIt(doIt)
{
    QString effectName;
    QDomNode namenode = effect.elementsByTagName("name").item(0);
    if (!namenode.isNull()) effectName = i18n(namenode.toElement().text().toUtf8().data());
    else effectName = i18n("effect");
    setText(i18n("Edit effect %1", effectName));
}

// virtual
int EditEffectCommand::id() const
{
    return 1;
}

// virtual
bool EditEffectCommand::mergeWith(const QUndoCommand * other)
{
    if (other->id() != id()) return false;
    if (m_track != static_cast<const EditEffectCommand*>(other)->m_track) return false;
    if (m_pos != static_cast<const EditEffectCommand*>(other)->m_pos) return false;
    m_effect = static_cast<const EditEffectCommand*>(other)->m_effect.cloneNode().toElement();
    return true;
}

// virtual
void EditEffectCommand::undo()
{
    m_view->updateEffect(m_track, m_pos, m_oldeffect, m_stackPos, false);
}
// virtual
void EditEffectCommand::redo()
{
    m_view->updateEffect(m_track, m_pos, m_effect, m_stackPos, m_doIt);
    m_doIt = false;
}


