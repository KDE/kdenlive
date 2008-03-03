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

#include "resizeclipcommand.h"

ResizeClipCommand::ResizeClipCommand(CustomTrackView *view, const QPointF startPos, const QPointF endPos, bool resizeClipStart, bool doIt)
        : m_view(view), m_startPos(startPos), m_endPos(endPos), m_resizeClipStart(resizeClipStart), m_doIt(doIt) {
    setText(i18n("Resize clip"));
}


// virtual
void ResizeClipCommand::undo() {
// kDebug()<<"----  undoing action";
    m_doIt = true;
    if (m_doIt) m_view->resizeClip(m_endPos, m_startPos, m_resizeClipStart);
}
// virtual
void ResizeClipCommand::redo() {
    kDebug() << "----  redoing action";
    if (m_doIt) m_view->resizeClip(m_startPos, m_endPos, m_resizeClipStart);
    m_doIt = true;
}

#include "resizeclipcommand.moc"
