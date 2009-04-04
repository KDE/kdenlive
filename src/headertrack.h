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

class HeaderTrack : public QWidget
{
    Q_OBJECT

public:
    HeaderTrack(int index, TrackInfo info, QWidget *parent = 0);
    ~HeaderTrack();
    void setLock(bool lock);

protected:
    //virtual void paintEvent(QPaintEvent * /*e*/);
    virtual void contextMenuEvent(QContextMenuEvent * event);

private:
    int m_index;
    TRACKTYPE m_type;
    Ui::TrackHeader_UI view;
    QMenu *m_contextMenu;

private slots:
    void switchAudio();
    void switchVideo();
    void slotDeleteTrack();
    void slotAddTrack();
    void slotChangeTrack();
    void switchLock(bool emitSignal = true);

signals:
    void switchTrackAudio(int);
    void switchTrackVideo(int);
    void switchTrackLock(int);
    void insertTrack(int);
    void deleteTrack(int);
    void changeTrack(int);
};

#endif
