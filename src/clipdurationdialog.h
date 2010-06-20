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


#ifndef CLIPDURATIONDIALOG_H
#define CLIPDURATIONDIALOG_H


#include "abstractclipitem.h"
#include "timecodedisplay.h"
#include "ui_clipdurationdialog_ui.h"

/**
 * @class ClipDurationDialog
 * @brief A dialog for modifying an item's (clip or transition) duration. 
 * @author Jean-Baptiste Mardelle
 */

class ClipDurationDialog : public QDialog, public Ui::ClipDurationDialog_UI
{
    Q_OBJECT

public:
    ClipDurationDialog(AbstractClipItem *clip, Timecode tc, GenTime min, GenTime max, QWidget * parent = 0);
    ~ClipDurationDialog();
    GenTime startPos() const;
    GenTime cropStart() const;
    GenTime duration() const;

private slots:
    void slotCheckDuration();
    void slotCheckStart();
    void slotCheckCrop();
    void slotCheckEnd();

private:
    AbstractClipItem *m_clip;
    TimecodeDisplay *m_pos;
    TimecodeDisplay *m_dur;
    TimecodeDisplay *m_cropStart;
    TimecodeDisplay *m_cropEnd;
    GenTime m_min;
    GenTime m_max;
    GenTime m_crop;
};


#endif

