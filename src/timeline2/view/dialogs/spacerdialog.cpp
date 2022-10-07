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
