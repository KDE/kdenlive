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

#include "addclipcommand.h"

AddClipCommand::AddClipCommand(ProjectList *list, const QStringList &names, const QDomElement &xml, const int id, const KUrl &url, const QString &group, bool doIt)
         : m_list(list), m_names(names), m_xml(xml), m_id(id), m_url(url), m_group(group), m_doIt(doIt) {
	    if (doIt) setText(i18n("Add clip"));
	    else setText(i18n("Delete clip"));
	  }


// virtual 
void AddClipCommand::undo()
{
  if (!m_list) kDebug()<<"----  ERROR, NO LIST FOR undoing action";
kDebug()<<"----  undoing action";
  if (m_doIt) m_list->deleteClip(m_id);
  else m_list->addClip(m_names, m_xml, m_id, m_url, m_group);
}
// virtual 
void AddClipCommand::redo()
{
  if (!m_list) kDebug()<<"----  ERROR, NO LIST FOR redoing action";
kDebug()<<"----  redoing action";
  if (m_doIt) m_list->addClip(m_names, m_xml, m_id, m_url, m_group);
  else m_list->deleteClip(m_id);
}

#include "addclipcommand.moc"
