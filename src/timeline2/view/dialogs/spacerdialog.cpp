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

#include "doc/kthumb.h"
#include "kdenlivesettings.h"

#include <QFontDatabase>
#include <QWheelEvent>

#include "klocalizedstring.h"

// deprecated
SpacerDialog::SpacerDialog(const GenTime &duration, const Timecode &tc, int track, const QList<TrackInfo> &tracks, QWidget *parent)
    : QDialog(parent)
    , m_in(tc)
{
    Q_UNUSED(track)
    Q_UNUSED(tracks)
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    inputLayout->addWidget(&m_in);
    m_in.setValue(duration);

    /*QIcon videoIcon = QIcon::fromTheme(QStringLiteral("kdenlive-show-video"));
    QIcon audioIcon = QIcon::fromTheme(QStringLiteral("kdenlive-show-audio"));
    track_number->addItem(i18n("All tracks"), -1);
    for (int i = tracks.count() - 1; i > 0; i--) {
        TrackInfo info = tracks.at(i);
        track_number->addItem(info.type == VideoTrack ? videoIcon : audioIcon, info.trackName.isEmpty() ? QString::number(i) : info.trackName, i);
    }
    track_number->setCurrentIndex(track == 0 ? 0 : tracks.count() - track);
    */
    adjustSize();
}

SpacerDialog::SpacerDialog(const GenTime &duration, const Timecode &tc, QWidget *parent)
    : QDialog(parent)
    , m_in(tc)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    inputLayout->addWidget(&m_in);
    m_in.setValue(duration);
    adjustSize();
}

GenTime SpacerDialog::selectedDuration() const
{
    return m_in.gentime();
}

bool SpacerDialog::affectAllTracks() const
{
    return insert_all_tracks->isChecked();
}

int SpacerDialog::selectedTrack() const
{
    return 0; // track_number->currentData().toInt();
}
