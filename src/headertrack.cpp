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
#include "kdenlivesettings.h"

#include <KIcon>
#include <KLocale>
#include <KDebug>

#include <QMouseEvent>
#include <QWidget>
#include <QPainter>
#include <QAction>

HeaderTrack::HeaderTrack(int index, TrackInfo info, QWidget *parent) :
        QWidget(parent),
        m_index(index),
        m_type(info.type)
{
    setFixedHeight(KdenliveSettings::trackheight());
    m_view.setupUi(this);
    m_view.track_number->setText(QString::number(m_index));
    m_view.buttonVideo->setChecked(!info.isBlind);
    m_view.buttonVideo->setToolTip(i18n("Hide track"));
    m_view.buttonAudio->setChecked(!info.isMute);
    m_view.buttonAudio->setToolTip(i18n("Mute track"));
    m_view.buttonLock->setChecked(info.isLocked);
    m_view.buttonLock->setToolTip(i18n("Lock track"));

    if (m_type == VIDEOTRACK) {
        m_view.frame->setBackgroundRole(QPalette::AlternateBase);
        m_view.frame->setAutoFillBackground(true);
        if (!info.isBlind) m_view.buttonVideo->setIcon(KIcon("kdenlive-show-video"));
        else m_view.buttonVideo->setIcon(KIcon("kdenlive-hide-video"));
    } else {
        m_view.buttonVideo->setHidden(true);
    }
    if (!info.isMute) m_view.buttonAudio->setIcon(KIcon("kdenlive-show-audio"));
    else m_view.buttonAudio->setIcon(KIcon("kdenlive-hide-audio"));

    if (!info.isLocked) m_view.buttonLock->setIcon(KIcon("kdenlive-unlock"));
    else m_view.buttonLock->setIcon(KIcon("kdenlive-lock"));

    connect(m_view.buttonVideo, SIGNAL(clicked()), this, SLOT(switchVideo()));
    connect(m_view.buttonAudio, SIGNAL(clicked()), this, SLOT(switchAudio()));
    connect(m_view.buttonLock, SIGNAL(clicked()), this, SLOT(switchLock()));

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
}

HeaderTrack::~HeaderTrack()
{
}

void HeaderTrack::switchVideo()
{
    if (m_view.buttonVideo->isChecked()) {
        m_view.buttonVideo->setIcon(KIcon("kdenlive-show-video"));
    } else {
        m_view.buttonVideo->setIcon(KIcon("kdenlive-hide-video"));
    }
    emit switchTrackVideo(m_index);
}

void HeaderTrack::switchAudio()
{
    if (m_view.buttonAudio->isChecked()) {
        m_view.buttonAudio->setIcon(KIcon("kdenlive-show-audio"));
    } else {
        m_view.buttonAudio->setIcon(KIcon("kdenlive-hide-audio"));
    }
    emit switchTrackAudio(m_index);
}

void HeaderTrack::switchLock(bool emitSignal)
{
    if (m_view.buttonLock->isChecked()) {
        m_view.buttonLock->setIcon(KIcon("kdenlive-lock"));
    } else {
        m_view.buttonLock->setIcon(KIcon("kdenlive-unlock"));
    }
    if (emitSignal) emit switchTrackLock(m_index);
}


void HeaderTrack::setLock(bool lock)
{
    m_view.buttonLock->setChecked(lock);
    switchLock(false);
}

void HeaderTrack::slotDeleteTrack()
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


// virtual
/*void HeaderTrack::paintEvent(QPaintEvent *e) {
    QRect region = e->rect();
    region.setTopLeft(QPoint(region.left() + 1, region.top() + 1));
    region.setBottomRight(QPoint(region.right() - 1, region.bottom() - 1));
    QPainter painter(this);
    if (m_type == AUDIOTRACK) painter.fillRect(region, QBrush(QColor(240, 240, 255)));
    else painter.fillRect(region, QBrush(QColor(255, 255, 255)));
    painter.drawText(region, Qt::AlignCenter, m_label);
}*/


#include "headertrack.moc"
