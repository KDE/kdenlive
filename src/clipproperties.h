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


#ifndef CLIPPROPSDIALOG_H
#define CLIPPROPSDIALOG_H

#include <QDialog>

#include "definitions.h"
#include "docclipbase.h"
#include "timecode.h"
#include "ui_clipproperties_ui.h"

class ClipProperties : public QDialog {
    Q_OBJECT

public:
    ClipProperties(DocClipBase *clip, Timecode tc, double fps, QWidget * parent = 0);


private slots:


private:
    Ui::ClipProperties_UI m_view;
    DocClipBase *m_clip;
    Timecode m_tc;
    double m_fps;
};


#endif

