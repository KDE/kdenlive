/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                 2012    Simon A. Eugster <simon.eu@gmail.com>           *
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


#include "moveclipcommand.h"
#include "customtrackview.h"

#include <KLocale>

MoveClipCommand::MoveClipCommand(CustomTrackView *view, const ItemInfo start, const ItemInfo end, bool doIt, QUndoCommand * parent) :
    QUndoCommand(parent),
    m_view(view),
    m_startPos(start),
    m_endPos(end),
    m_doIt(doIt),
    m_success(true)
{
    setText(i18n("Move clip"));
    if (parent) {
        // command has a parent, so there are several operations ongoing, do not refresh monitor
        m_refresh = false;
    } else m_refresh = true;
}


void MoveClipCommand::undo()
{
    m_doIt = true;
    // We can only undo what was done;
    // if moveClip() failed in redo() the document does (or should) not change.
    if (m_success) {
        m_view->moveClip(m_endPos, m_startPos, m_refresh);
    }
}
void MoveClipCommand::redo()
{
    if (m_doIt) {
//        qDebug() << "Executing move clip command. End now:" << m_endPos;
        m_success = m_view->moveClip(m_startPos, m_endPos, m_refresh, &m_endPos);
//        qDebug() << "Move clip command executed. End now: " << m_endPos;
    }
    m_doIt = true;
}

