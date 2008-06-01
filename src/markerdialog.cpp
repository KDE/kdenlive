/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include <KDebug>

#include "markerdialog.h"

MarkerDialog::MarkerDialog(DocClipBase *clip, CommentedTime t, Timecode tc, QWidget * parent): QDialog(parent), m_tc(tc), m_clip(clip), m_marker(t) {
    setFont(KGlobalSettings::toolBarFont());
    m_fps = m_tc.fps();
    m_view.setupUi(this);
    m_view.marker_position->setText(tc.getTimecode(t.time(), tc.fps()));
    m_view.marker_comment->setText(t.comment());
    connect(m_view.position_up, SIGNAL(clicked()), this, SLOT(slotTimeUp()));
    connect(m_view.position_down, SIGNAL(clicked()), this, SLOT(slotTimeDown()));
    m_view.marker_comment->selectAll();
    m_view.marker_comment->setFocus();
    adjustSize();
}


void MarkerDialog::slotTimeUp() {
    int duration = m_tc.getFrameCount(m_view.marker_position->text(), m_fps);
    if (duration >= m_clip->duration().frames(m_fps)) return;
    duration ++;
    m_view.marker_position->setText(m_tc.getTimecode(GenTime(duration, m_fps), m_fps));
}

void MarkerDialog::slotTimeDown() {
    int duration = m_tc.getFrameCount(m_view.marker_position->text(), m_fps);
    if (duration <= 0) return;
    duration --;
    m_view.marker_position->setText(m_tc.getTimecode(GenTime(duration, m_fps), m_fps));
}

CommentedTime MarkerDialog::newMarker() {
    return CommentedTime(GenTime(m_tc.getFrameCount(m_view.marker_position->text(), m_fps), m_fps), m_view.marker_comment->text());
}

#include "markerdialog.moc"


