/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle (jb@kdenlive.org)

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef SPACERDIALOG_H
#define SPACERDIALOG_H

#include "definitions.h"
#include "timecode.h"
#include "timecodedisplay.h"
#include "ui_spacerdialog_ui.h"

/** @class SpacerDialog
    @brief A dialog to specify length and track of inserted space.
    @author Jean-Baptiste Mardelle
 */
class SpacerDialog : public QDialog, public Ui::SpacerDialog_UI
{
    Q_OBJECT

public:
    SpacerDialog(const GenTime &duration, const Timecode &tc, int track, const QList<TrackInfo> &tracks, QWidget *parent = nullptr);
    SpacerDialog(const GenTime &duration, const Timecode &tc, QWidget *parent = nullptr);
    GenTime selectedDuration() const;
    int selectedTrack() const;
    bool affectAllTracks() const;

private:
    TimecodeDisplay m_in;
};

#endif
