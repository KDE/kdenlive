/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "spacerdialog.h"

#include "doc/kthumb.h"
#include "kdenlivesettings.h"

#include <QFontDatabase>
#include <QWheelEvent>

#include "klocalizedstring.h"

// deprecated
SpacerDialog::SpacerDialog(const GenTime &duration, const Timecode &tc, int track, const QList<TrackInfo> &tracks, QWidget *parent)
    : QDialog(parent)
    , m_in(nullptr, tc)
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
    , m_in(nullptr, tc)
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
