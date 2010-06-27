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

#ifndef HEADERTRACK_H
#define HEADERTRACK_H

#include <QContextMenuEvent>
#include <QMenu>

#include "definitions.h"
#include "ui_trackheader_ui.h"

class HeaderTrack : public QWidget, public Ui::TrackHeader_UI
{
    Q_OBJECT

public:
    HeaderTrack(int index, TrackInfo info, int height, QWidget *parent = 0);
    //virtual ~HeaderTrack();
    void setLock(bool lock);
    void adjustSize(int height);
    void setSelectedIndex(int ix);

protected:
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseDoubleClickEvent(QMouseEvent * event);

private:
    int m_index;
    TRACKTYPE m_type;

private slots:
    void switchAudio();
    void switchVideo();
    void slotDeleteTrack();
    void deleteTrack();
    void slotAddTrack();
    void slotRenameTrack();
    void slotConfigTrack();
    void switchLock(bool emitSignal = true);

signals:
    void switchTrackAudio(int);
    void switchTrackVideo(int);
    void switchTrackLock(int);
    void insertTrack(int);
    void deleteTrack(int);
    void renameTrack(int, QString);
    void selectTrack(int);
    void configTrack(int);
};

#endif
