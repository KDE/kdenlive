/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include "abstractmonitor.h"
#include "kdenlivesettings.h"

#include <KDebug>

#include <QPaintEvent>

VideoPreviewContainer::VideoPreviewContainer(QWidget *parent) :
    QFrame(parent),
    m_dar(1.0),
    m_refresh(false)
{
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::ClickFocus);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(&m_refreshTimer, SIGNAL(timeout()), this, SLOT(update()));
    m_refreshTimer.setSingleShot(false);
    m_refreshTimer.setInterval(200);
}

VideoPreviewContainer::~VideoPreviewContainer()
{
    qDeleteAll(m_imageQueue);
}

//virtual
void VideoPreviewContainer::resizeEvent( QResizeEvent * /*event*/ )
{
    updateDisplayZone();
}

void VideoPreviewContainer::setRatio(double ratio)
{
    m_dar = ratio;
    updateDisplayZone();
}


void VideoPreviewContainer::updateDisplayZone()
{
    QRect rect = this->frameRect();
    int paintW = rect.height() * m_dar + 0.5;
    if (paintW > rect.width()) {
        int paintH = rect.width() / m_dar + 0.5;
        int diff = (rect.height() - paintH)  / 2;
        rect.adjust(0, diff, 0, 0);
        rect.setHeight(paintH);
    }
    else {
        int diff = (rect.width() - paintW)  / 2;
        rect.adjust(diff, 0, 0, 0);
        rect.setWidth(paintW);
    }
    m_displayRect = rect;
    m_refresh = true;
}

void VideoPreviewContainer::setImage(QImage img)
{
    if (m_imageQueue.count() > 2) {
        delete m_imageQueue.takeLast();
    }
    m_imageQueue.prepend(new QImage(img));
    update();
}

void VideoPreviewContainer::stop()
{
    //m_refreshTimer.stop();
    qDeleteAll(m_imageQueue);
    m_imageQueue.clear();
}

void VideoPreviewContainer::start()
{
    //m_refreshTimer.start();
}

// virtual
void VideoPreviewContainer::paintEvent(QPaintEvent *event)
{
    if (m_imageQueue.isEmpty()) return;
    QImage *img = m_imageQueue.takeFirst();
    QPainter painter(this);
    if (m_refresh) {
        painter.fillRect(event->rect(), QColor(KdenliveSettings::window_background()));
        m_refresh = false;
    }
    painter.drawImage(m_displayRect, *img);
    delete img;
}


