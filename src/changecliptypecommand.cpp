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


#include "changecliptypecommand.h"
#include "customtrackview.h"

#include <KLocale>

ChangeClipTypeCommand::ChangeClipTypeCommand(CustomTrackView *view, const int track, const GenTime &pos, bool videoOnly, bool audioOnly, bool originalVideo, bool originalAudio, QUndoCommand * parent) :
        QUndoCommand(parent),
        m_view(view),
        m_pos(pos),
        m_track(track),
        m_videoOnly(videoOnly),
        m_audioOnly(audioOnly),
        m_originalVideoOnly(originalVideo),
        m_originalAudioOnly(originalAudio)
{
    setText(i18n("Change clip type"));
}

// virtual
void ChangeClipTypeCommand::undo()
{
// kDebug()<<"----  undoing action";
    m_view->doChangeClipType(m_pos, m_track, m_originalVideoOnly, m_originalAudioOnly);
}
// virtual
void ChangeClipTypeCommand::redo()
{
    kDebug() << "----  redoing action";
        m_view->doChangeClipType(m_pos, m_track, m_videoOnly, m_audioOnly);
}

