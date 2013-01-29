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


SpacerDialog::SpacerDialog(const GenTime duration, Timecode tc, int track, QList <TrackInfo> tracks, QWidget * parent) :
        QDialog(parent),
        m_in(tc)
{
    setFont(KGlobalSettings::toolBarFont());
    setupUi(this);
    inputLayout->addWidget(&m_in);
    m_in.setValue(duration);

    QStringList trackItems;
    trackItems << i18n("All tracks");
    for (int i = tracks.count() - 1; i >= 0; --i) {
        if (!tracks.at(i).trackName.isEmpty())
            trackItems << tracks.at(i).trackName + " (" + QString::number(i) + ')';
        else
            trackItems << QString::number(i);
    }
    track_number->addItems(trackItems);
    track_number->setCurrentIndex(track);

    adjustSize();
}

GenTime SpacerDialog::selectedDuration()
{
    return m_in.gentime();
}

int SpacerDialog::selectedTrack()
{
    return track_number->currentIndex() - 1;
}

#include "spacerdialog.moc"


