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

#include <QDomElement>

#include "definitions.h"
#include "ui_trackheader_ui.h"

class HeaderTrack : public QWidget, public Ui::TrackHeader_UI
{
    Q_OBJECT

public:
    HeaderTrack(int index, TrackInfo info, int height, const QList <QAction *>& actions, QWidget *parent = 0);
    //virtual ~HeaderTrack();
    void setLock(bool lock);
    void adjustSize(int height);
    void setSelectedIndex(int ix);
    /** @brief Update the track label to show if current track has effects or not.*/
    void updateEffectLabel(const QStringList &effects);

protected:
    void mousePressEvent(QMouseEvent * event);
    void mouseDoubleClickEvent(QMouseEvent * event);
    void dropEvent(QDropEvent * event);
    void dragEnterEvent(QDragEnterEvent *event);

private:
    int m_index;
    TrackType m_type;
    bool m_isSelected;
    QString m_name;

private slots:
    void switchAudio();
    void switchVideo();
    void slotRenameTrack();
    void switchLock(bool emitSignal = true);

signals:
    void switchTrackAudio(int);
    void switchTrackVideo(int);
    void switchTrackLock(int);
    void renameTrack(int, const QString&);
    void selectTrack(int);
    void configTrack(int);
    void addTrackEffect(const QDomElement &, int);
    void showTrackEffects(int);
};

#endif
