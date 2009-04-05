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


#include "spacerdialog.h"
#include "kthumb.h"
#include "kdenlivesettings.h"

#include <QWheelEvent>
#include <KDebug>


SpacerDialog::SpacerDialog(const GenTime duration, Timecode tc, int track, int trackNumber, QWidget * parent) :
        QDialog(parent),
        m_tc(tc)
{
    setFont(KGlobalSettings::toolBarFont());
    m_fps = m_tc.fps();
    m_view.setupUi(this);
    m_view.space_duration->setText(tc.getTimecode(duration, m_fps));
    QStringList tracks;
    tracks << i18n("All tracks");
    for (int i = 0; i < trackNumber - 1; i++) {
        tracks << QString::number(i);
    }
    m_view.track_number->addItems(tracks);
    m_view.track_number->setCurrentIndex(track);

    connect(m_view.position_up, SIGNAL(clicked()), this, SLOT(slotTimeUp()));
    connect(m_view.position_down, SIGNAL(clicked()), this, SLOT(slotTimeDown()));

    adjustSize();
}

SpacerDialog::~SpacerDialog()
{
}

void SpacerDialog::slotTimeUp()
{
    int duration = m_tc.getFrameCount(m_view.space_duration->text(), m_fps);
    duration ++;
    m_view.space_duration->setText(m_tc.getTimecode(GenTime(duration, m_fps), m_fps));
}

void SpacerDialog::slotTimeDown()
{
    int duration = m_tc.getFrameCount(m_view.space_duration->text(), m_fps);
    if (duration <= 0) return;
    duration --;
    m_view.space_duration->setText(m_tc.getTimecode(GenTime(duration, m_fps), m_fps));
}

GenTime SpacerDialog::selectedDuration()
{
    return GenTime(m_tc.getFrameCount(m_view.space_duration->text(), m_fps), m_fps);
}

void SpacerDialog::wheelEvent(QWheelEvent * event)
{
    if (m_view.space_duration->underMouse()) {
        if (event->delta() > 0)
            slotTimeUp();
        else
            slotTimeDown();
    }
}

int SpacerDialog::selectedTrack()
{
    return m_view.track_number->currentIndex() - 1;
}

#include "spacerdialog.moc"


