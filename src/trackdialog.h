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


#ifndef TRACKDIALOG_H
#define TRACKDIALOG_H


#include "ui_addtrack_ui.h"

class KdenliveDoc;

class TrackDialog : public QDialog
{
    Q_OBJECT

public:
    TrackDialog(KdenliveDoc *doc, QWidget * parent = 0);
    ~TrackDialog();
    Ui::AddTrack_UI view;

public slots:

    void slotUpdateName(int);

private:
    KdenliveDoc *m_doc;
};


#endif

