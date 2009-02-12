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

#include <QDialog>

#include "abstractclipitem.h"
#include "timecode.h"
#include "ui_clipdurationdialog_ui.h"


class ClipDurationDialog : public QDialog {
    Q_OBJECT

public:
    ClipDurationDialog(AbstractClipItem *clip, Timecode tc, QWidget * parent = 0);
    ~ClipDurationDialog();
    GenTime startPos() const;
    GenTime cropStart() const;
    GenTime duration() const;
    void setMargins(GenTime min, GenTime max);

protected:
    void wheelEvent(QWheelEvent * event);

private slots:
    void slotPosUp();
    void slotPosDown();
    void slotDurUp();
    void slotDurDown();
    void slotCropUp();
    void slotCropDown();
    void slotCheckDuration();
    void slotCheckStart();
    void slotCheckCrop();

private:
    Ui::ClipDurationDialog_UI m_view;
    AbstractClipItem *m_clip;
    Timecode m_tc;
    double m_fps;
    GenTime m_min;
    GenTime m_max;
};


#endif

