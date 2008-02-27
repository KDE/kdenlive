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

#include "editeffectcommand.h"

EditEffectCommand::EditEffectCommand(CustomTrackView *view, const int track, GenTime pos, QDomElement oldeffect, QDomElement effect, bool doIt)
         : m_view(view), m_track(track), m_pos(pos), m_oldeffect(oldeffect), m_doIt(doIt) {
	    m_effect = effect.cloneNode().toElement();
	    setText(i18n("Edit effect"));
	 }

// virtual
int EditEffectCommand::id() const
{
  return 1;
}

// virtual
bool EditEffectCommand::mergeWith ( const QUndoCommand * other )
{
  if (other->id() != id()) return false;
  if (m_track != static_cast<const EditEffectCommand*>(other)->m_track) return false;
  if (m_pos != static_cast<const EditEffectCommand*>(other)->m_pos) return false;
  m_effect = static_cast<const EditEffectCommand*>(other)->m_effect;
  return true;
}

// virtual 
void EditEffectCommand::undo()
{
kDebug()<<"----  undoing action";
  m_view->updateEffect(m_track, m_pos, m_oldeffect);
}
// virtual 
void EditEffectCommand::redo()
{
kDebug()<<"----  redoing action";
  m_view->updateEffect(m_track, m_pos, m_effect);
}

#include "editeffectcommand.moc"
