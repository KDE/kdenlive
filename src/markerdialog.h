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

#include <QDialog>

#include "docclipbase.h"
#include "timecode.h"
#include "ui_markerdialog_ui.h"

namespace Mlt {
class Producer;
class Profile;
};

class MarkerDialog : public QDialog {
    Q_OBJECT

public:
    MarkerDialog(DocClipBase *clip, CommentedTime t, Timecode tc, QWidget * parent = 0);
    ~MarkerDialog();
    CommentedTime newMarker();

private slots:
    void slotTimeUp();
    void slotTimeDown();
    void slotUpdateThumb();

protected:
    void wheelEvent(QWheelEvent * event);

private:
    Mlt::Producer *m_producer;
    Mlt::Profile *m_profile;
    Ui::MarkerDialog_UI m_view;
    DocClipBase *m_clip;
    CommentedTime m_marker;
    Timecode m_tc;
    double m_fps;
    double m_dar;
    QTimer *m_previewTimer;

signals:
    void updateThumb();
};


#endif

