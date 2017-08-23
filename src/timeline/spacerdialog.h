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

#ifndef SPACERDIALOG_H
#define SPACERDIALOG_H

#include "definitions.h"
#include "timecode.h"
#include "timecodedisplay.h"
#include "ui_spacerdialog_ui.h"

/**
 * @class SpacerDialog
 * @brief A dialog to specify length and track of inserted space.
 * @author Jean-Baptiste Mardelle
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
