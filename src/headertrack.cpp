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


#include "headertrack.h"

#include <KIcon>
#include <KLocale>
#include <KDebug>

#include <QMouseEvent>
#include <QWidget>
#include <QPainter>
#include <QAction>
#include <QTimer>

HeaderTrack::HeaderTrack(int index, TrackInfo info, int height, QWidget *parent) :
        QWidget(parent),
        m_index(index),
        m_type(info.type)
{
    setFixedHeight(height);
    setupUi(this);
    track_number->setText(info.trackName.isEmpty() ? QString::number(m_index) : info.trackName);

    buttonVideo->setChecked(info.isBlind);
    buttonVideo->setToolTip(i18n("Hide track"));
    buttonAudio->setChecked(info.isMute);
    buttonAudio->setToolTip(i18n("Mute track"));
    buttonLock->setChecked(info.isLocked);
    buttonLock->setToolTip(i18n("Lock track"));

    if (m_type == VIDEOTRACK) {
        setBackgroundRole(QPalette::AlternateBase);
        setAutoFillBackground(true);
        if (!info.isBlind) buttonVideo->setIcon(KIcon("kdenlive-show-video"));
        else buttonVideo->setIcon(KIcon("kdenlive-hide-video"));
    } else {
        buttonVideo->setHidden(true);
    }
    if (!info.isMute) buttonAudio->setIcon(KIcon("kdenlive-show-audio"));
    else buttonAudio->setIcon(KIcon("kdenlive-hide-audio"));

    if (!info.isLocked) buttonLock->setIcon(KIcon("kdenlive-unlock"));
    else buttonLock->setIcon(KIcon("kdenlive-lock"));

    connect(buttonVideo, SIGNAL(clicked()), this, SLOT(switchVideo()));
    connect(buttonAudio, SIGNAL(clicked()), this, SLOT(switchAudio()));
    connect(buttonLock, SIGNAL(clicked()), this, SLOT(switchLock()));

    // Don't show track buttons if size is too small
    if (height < 40) {
        buttonVideo->setHidden(true);
        buttonAudio->setHidden(true);
        buttonLock->setHidden(true);
        //horizontalSpacer;
    }

    setContextMenuPolicy(Qt::ActionsContextMenu);
    QAction *insertAction = new QAction(i18n("Insert Track"), this);
    addAction(insertAction);
    connect(insertAction, SIGNAL(triggered()), this, SLOT(slotAddTrack()));

    QAction *removeAction = new QAction(KIcon("edit-delete"), i18n("Delete Track"), this);
    addAction(removeAction);
    connect(removeAction, SIGNAL(triggered()), this, SLOT(slotDeleteTrack()));

    QAction *changeAction = new QAction(i18n("Change Track Type"), this);
    addAction(changeAction);
    connect(changeAction, SIGNAL(triggered()), this, SLOT(slotChangeTrack()));

    QAction *renameAction = new QAction(i18n("Rename Track"), this);
    addAction(renameAction);
    connect(renameAction, SIGNAL(triggered()), this, SLOT(slotRenameTrack()));

}

/*HeaderTrack::~HeaderTrack()
{
}*/

void HeaderTrack::adjustSize(int height)
{
    // Don't show track buttons if size is too small
    bool smallTracks = height < 40;
    if (m_type == VIDEOTRACK) buttonVideo->setHidden(smallTracks);
    buttonAudio->setHidden(smallTracks);
    buttonLock->setHidden(smallTracks);
    setFixedHeight(height);
}

void HeaderTrack::switchVideo()
{
    if (buttonVideo->isChecked()) {
        buttonVideo->setIcon(KIcon("kdenlive-hide-video"));
    } else {
        buttonVideo->setIcon(KIcon("kdenlive-show-video"));
    }
    emit switchTrackVideo(m_index);
}

void HeaderTrack::switchAudio()
{
    if (buttonAudio->isChecked()) {
        buttonAudio->setIcon(KIcon("kdenlive-hide-audio"));
    } else {
        buttonAudio->setIcon(KIcon("kdenlive-show-audio"));
    }
    emit switchTrackAudio(m_index);
}

void HeaderTrack::switchLock(bool emitSignal)
{
    if (buttonLock->isChecked()) {
        buttonLock->setIcon(KIcon("kdenlive-lock"));
    } else {
        buttonLock->setIcon(KIcon("kdenlive-unlock"));
    }
    if (emitSignal) emit switchTrackLock(m_index);
}


void HeaderTrack::setLock(bool lock)
{
    buttonLock->setChecked(lock);
    switchLock(false);
}

void HeaderTrack::slotDeleteTrack()
{
    QTimer::singleShot(500, this, SLOT(deleteTrack()));
}

void HeaderTrack::deleteTrack()
{
    emit deleteTrack(m_index);
}

void HeaderTrack::slotAddTrack()
{
    emit insertTrack(m_index);
}

void HeaderTrack::slotChangeTrack()
{
    emit changeTrack(m_index);
}

void HeaderTrack::slotRenameTrack()
{
    emit renameTrack(m_index);
}


#include "headertrack.moc"
