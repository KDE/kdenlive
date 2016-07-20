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

class KDualAction;
class QToolBar;
class Track;

class HeaderTrack : public QWidget, public Ui::TrackHeader_UI
{
    Q_OBJECT

public:
    HeaderTrack(TrackInfo info, const QList <QAction *>& actions, Track *parent, int height, QWidget *parentWidget);
    virtual ~HeaderTrack();
    bool isTarget;
    void setLock(bool lock);
    void setComposite(bool enable);
    void adjustSize(int height);
    void setSelectedIndex(int ix);
    /** @brief Update the track label to show if current track has effects or not.*/
    void updateEffectLabel(const QStringList &effects);
    void renameTrack(const QString &name);
    QString name() const;
    /** @brief Update status of mute/blind/lock/composite buttons.*/
    void updateStatus(TrackInfo info);
    /** @brief Track has no composite transition, disable switch button.*/
    void disableComposite();
    void refreshPalette();
    void switchTarget(bool enable);
    void updateLed();
    void setVideoMute(bool mute);
    void setAudioMute(bool mute);
    bool compositeEnabled() const;

protected:
    void mousePressEvent(QMouseEvent * event);
    void mouseDoubleClickEvent(QMouseEvent * event);
    void dropEvent(QDropEvent * event);
    void dragEnterEvent(QDragEnterEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);

private:
    TrackType m_type;
    Track *m_parentTrack;
    bool m_isSelected;
    QString m_name;
    QToolBar *m_tb;
    KDualAction *m_switchComposite;
    KDualAction *m_switchVideo;
    KDualAction *m_switchAudio;
    KDualAction *m_switchLock;
    void updateBackground(bool isLocked);

private slots:
    void switchAudio(bool);
    void switchVideo(bool);
    void switchComposite(bool);
    void slotRenameTrack();
    void switchLock(bool);

signals:
    void switchTrackAudio(int index,bool);
    void switchTrackVideo(int index,bool);
    void switchTrackComposite(int index,bool);
    void switchTrackLock(int index,bool);
    void renameTrack(int index, const QString&name);
    void selectTrack(int index, bool switchTarget = false);
    void configTrack();
    void addTrackEffect(const QDomElement &, int index);
};

#endif
