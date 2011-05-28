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

#include <KDebug>

VideoPreviewContainer::VideoPreviewContainer(QWidget *parent) :
    QFrame(parent)
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


void VideoPreviewContainer::setImage(QImage img)
{
    if (m_imageQueue.count() > 2) {
        delete m_imageQueue.takeLast();//replace(10, new QImage(img));
    }
    m_imageQueue.prepend(new QImage(img));
}

void VideoPreviewContainer::stop()
{
    m_refreshTimer.stop();
    qDeleteAll(m_imageQueue);
    m_imageQueue.clear();
}

void VideoPreviewContainer::start()
{
    m_refreshTimer.start();
}

// virtual
void VideoPreviewContainer::paintEvent(QPaintEvent */*event*/)
{
        if (m_imageQueue.isEmpty()) return;
        QImage *img = m_imageQueue.takeFirst();
        QPainter painter(this);
        double ar = (double) img->width() / img->height();
        QRect rect = this->frameRect();
        int paintW = rect.height() * ar + 0.5;
        if (paintW > rect.width()) {
            paintW = rect.width() / ar + 0.5;
            int diff = (rect.height() - paintW)  / 2;
            rect.adjust(0, diff, 0, 0);
            rect.setHeight(paintW);
        }
        else {
            int diff = (rect.width() - paintW)  / 2;
            rect.adjust(diff, 0, 0, 0);
            rect.setWidth(paintW);
        }

        painter.drawImage(rect, *img);
        delete img;
}


