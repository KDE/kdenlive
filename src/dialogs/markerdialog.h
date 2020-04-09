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

#ifndef MARKERDIALOG_H
#define MARKERDIALOG_H

#include "ui_markerdialog_ui.h"

#include "definitions.h"
#include "timecode.h"
#include "timecodedisplay.h"

class ClipController;

namespace Mlt {
}

/**
 * @class MarkerDialog
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */

class MarkerDialog : public QDialog, public Ui::MarkerDialog_UI
{
    Q_OBJECT

public:
    explicit MarkerDialog(ClipController *clip, const CommentedTime &t, const Timecode &tc, const QString &caption, QWidget *parent = nullptr);
    ~MarkerDialog() override;

    CommentedTime newMarker();
    QImage markerImage() const;

private slots:
    void slotUpdateThumb();

private:
    ClipController *m_clip;
    TimecodeDisplay *m_in;
    QTimer *m_previewTimer;

signals:
    void updateThumb();
};

#endif
