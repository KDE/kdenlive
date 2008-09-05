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

#include <QMouseEvent>
#include <QStylePainter>
#include <QFrame>
#include <QWidget>
#include <QPainter>
#include <QAction>

#include <KIcon>
#include <KLocale>
#include <KDebug>

#include "kdenlivesettings.h"
#include "headertrack.h"

HeaderTrack::HeaderTrack(int index, TrackInfo info, QWidget *parent)
        : QWidget(parent), m_index(index), m_type(info.type) {
    setFixedHeight(KdenliveSettings::trackheight());
    view.setupUi(this);
    view.track_number->setText(QString::number(m_index));
    if (m_type == VIDEOTRACK) {
        view.frame->setBackgroundRole(QPalette::AlternateBase);
        view.frame->setAutoFillBackground(true);
        view.buttonVideo->setIcon(KIcon("kdenlive-show-video"));
    } else {
        view.buttonVideo->setHidden(true);
    }
    view.buttonAudio->setIcon(KIcon("kdenlive-show-audio"));
    view.buttonVideo->setChecked(!info.isBlind);
    view.buttonAudio->setChecked(!info.isMute);
    connect(view.buttonVideo, SIGNAL(clicked()), this, SLOT(switchVideo()));
    connect(view.buttonAudio, SIGNAL(clicked()), this, SLOT(switchAudio()));

    m_contextMenu = new QMenu(this);
    QAction *insertAction = new QAction(i18n("Insert track"), this);
    m_contextMenu->addAction(insertAction);
    connect(insertAction, SIGNAL(triggered(QAction *)), this, SLOT(slotAddTrack()));

    QAction *removeAction = new QAction(KIcon("edit-delete"), i18n("Delete track"), this);
    m_contextMenu->addAction(removeAction);
    connect(removeAction, SIGNAL(triggered(QAction *)), this, SLOT(slotDeleteTrack()));
}

void HeaderTrack::switchVideo() {
    emit switchTrackVideo(m_index);
}

void HeaderTrack::switchAudio() {
    emit switchTrackAudio(m_index);
}

void HeaderTrack::slotDeleteTrack() {
    emit deleteTrack(m_index);
}

void HeaderTrack::slotAddTrack() {
    emit insertTrack(m_index);
}

// virtual
void HeaderTrack::contextMenuEvent(QContextMenuEvent * event) {
    m_contextMenu->popup(event->globalPos());
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
