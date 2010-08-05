/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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


#include "monitorscene.h"
#include "renderer.h"
#include "kdenlivesettings.h"

#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QtCore>

MonitorScene::MonitorScene(Render *renderer, QObject* parent) :
        QGraphicsScene(parent),
        m_renderer(renderer)
{
}

void MonitorScene::setUp()
{
    setBackgroundBrush(QBrush(QColor(KdenliveSettings::window_background().name())));

    QPen framepen(Qt::DotLine);
    framepen.setColor(Qt::red);

    m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, m_renderer->renderWidth(), m_renderer->renderHeight()));
    m_frameBorder->setPen(framepen);
    m_frameBorder->setZValue(-9);
    m_frameBorder->setBrush(Qt::transparent);
    m_frameBorder->setFlags(0);
    addItem(m_frameBorder);

    m_lastUpdate.start();
    m_background = new QGraphicsPixmapItem();
    m_background->setZValue(-10);
    m_background->setFlags(0);
    QPixmap bg(m_renderer->renderWidth(), m_renderer->renderHeight());
    bg.fill();
    m_background->setPixmap(bg);
    addItem(m_background);

    connect(m_renderer, SIGNAL(rendererPosition(int)), this, SLOT(slotUpdateBackground()));
    connect(m_renderer, SIGNAL(frameUpdated(int)), this, SLOT(slotUpdateBackground()));
    slotUpdateBackground();
}

void MonitorScene::slotUpdateBackground()
{
    if (views().count() > 0 && views().at(0)->isVisible()) {
        if (m_lastUpdate.elapsed() > 200) {
            m_background->setPixmap(QPixmap::fromImage(m_renderer->extractFrame(m_renderer->seekFramePosition())));
            views().at(0)->fitInView(m_frameBorder, Qt::KeepAspectRatio);
            views().at(0)->centerOn(m_frameBorder);
            m_lastUpdate.start();
        }
    }
}

void MonitorScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mousePressEvent(event);
}


void MonitorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mouseReleaseEvent(event);
    emit actionFinished();
}

#include "monitorscene.moc"
