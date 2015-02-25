/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *               2013 by Jean-Nicolas Artaud (jeannicolasartaud@gmail.com) *
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

#include "videosurface.h"


VideoSurface::VideoSurface(QWidget* parent) :
    QWidget(parent)
{
    // MonitorRefresh is used as container for the SDL display (it's window id is passed to SDL)
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setAttribute(Qt::WA_NoSystemBackground);
    //setUpdatesEnabled(false);
}

void VideoSurface::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    //WARNING: This might trigger unnecessary refreshes from MLT's producer, but without this,
    // as soon as monitor is covered by a popup menu or another window, image is corrupted.
    emit refreshMonitor();
}

