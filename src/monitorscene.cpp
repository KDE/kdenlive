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
#include <QGraphicsSceneMouseEvent>

MonitorScene::MonitorScene(Render *renderer, QObject* parent) :
        QGraphicsScene(parent),
        m_renderer(renderer),
        m_view(NULL),
        m_selectedItem(NULL),
        m_resizeMode(NoResize),
        m_clickPoint(0, 0),
        m_backgroundImage(QImage()),
        m_enabled(true),
        m_modified(false),
        m_directUpdate(false)
{
    setBackgroundBrush(QBrush(QColor(KdenliveSettings::window_background().name())));

    QPen framepen(Qt::SolidLine);
    framepen.setColor(Qt::red);

    m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, m_renderer->renderWidth(), m_renderer->renderHeight()));
    m_frameBorder->setPen(framepen);
    m_frameBorder->setZValue(-2);
    m_frameBorder->setBrush(Qt::transparent);
    m_frameBorder->setFlags(0);
    addItem(m_frameBorder);

    m_lastUpdate.start();
    m_background = new QGraphicsPixmapItem();
    m_background->setZValue(-1);
    m_background->setFlags(0);
    m_background->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    m_background->setTransformationMode(Qt::FastTransformation);
    QPixmap bg(m_renderer->renderWidth(), m_renderer->renderHeight());
    bg.fill();
    m_background->setPixmap(bg);
    addItem(m_background);

    //connect(m_renderer, SIGNAL(rendererPosition(int)), this, SLOT(slotUpdateBackground()));
    connect(m_renderer, SIGNAL(frameUpdated(QImage)), this, SLOT(slotSetBackgroundImage(QImage)));
}

void MonitorScene::setUp()
{
    if (views().count() > 0)
        m_view = views().at(0);
    else
        m_view = NULL;

    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    slotUpdateBackground(true);
}

void MonitorScene::resetProfile()
{
    const QRectF border(0, 0, m_renderer->renderWidth(), m_renderer->renderHeight());
    m_frameBorder->setRect(border);
}

void MonitorScene::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void MonitorScene::slotUpdateBackground(bool fit)
{
    if (m_view && m_view->isVisible()) {
        if (m_lastUpdate.elapsed() > 200) {
            m_background->setPixmap(QPixmap::fromImage(m_backgroundImage, Qt::ThresholdDither));
            if (fit) {
                m_view->fitInView(m_frameBorder, Qt::KeepAspectRatio);
                m_view->centerOn(m_frameBorder);
            }
            m_lastUpdate.start();
        }
    }
}

void MonitorScene::slotSetDirectUpdate(bool directUpdate)
{
    m_directUpdate = directUpdate;
}

bool MonitorScene::getDirectUpdate()
{
    return m_directUpdate;
}

void MonitorScene::slotSetBackgroundImage(QImage image)
{
    m_backgroundImage = image;
    slotUpdateBackground();
}

resizeModes MonitorScene::getResizeMode(QGraphicsRectItem *item, QPoint pos)
{
    if (!m_view)
        return NoResize;

    QRectF rect = item->rect().normalized();
    // Item mapped coordinates
    QPolygon pol = item->deviceTransform(m_view->viewportTransform()).map(rect).toPolygon();
    QPainterPath top(pol.point(0));
    top.lineTo(pol.point(1));
    QPainterPath bottom(pol.point(2));
    bottom.lineTo(pol.point(3));
    QPainterPath left(pol.point(0));
    left.lineTo(pol.point(3));
    QPainterPath right(pol.point(1));
    right.lineTo(pol.point(2));

    QPainterPath mouseArea;
    mouseArea.addRect(pos.x() - 4, pos.y() - 4, 8, 8);

    // Check for collisions between the mouse and the borders
    if (mouseArea.contains(pol.point(0)))
        return TopLeft;
    else if (mouseArea.contains(pol.point(2)))
        return BottomRight;
    else if (mouseArea.contains(pol.point(1)))
        return TopRight;
    else if (mouseArea.contains(pol.point(3)))
        return BottomLeft;
    else if (top.intersects(mouseArea))
        return Top;
    else if (bottom.intersects(mouseArea))
        return Bottom;
    else if (right.intersects(mouseArea))
        return Right;
    else if (left.intersects(mouseArea))
        return Left;
    else
        return NoResize;
}

void MonitorScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_enabled)
        return;

    m_resizeMode = NoResize;
    m_selectedItem = NULL;

    m_clickPoint = event->scenePos();
    QList<QGraphicsItem *> itemList = items(QRectF(m_clickPoint - QPoint(4, 4), QSizeF(4, 4)).toRect());

    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->zValue() >= 0 && itemList.at(i)->flags() & QGraphicsItem::ItemIsMovable) {
            m_selectedItem = itemList.at(i);
            // Rect
            if (itemList.at(i)->type() == 3) {
                m_resizeMode = getResizeMode((QGraphicsRectItem*)m_selectedItem, m_view->mapFromScene(m_clickPoint));
                break;
            }
        }
    }

    QGraphicsScene::mousePressEvent(event);
}

void MonitorScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_enabled)
        return;

    QPointF mousePos = event->scenePos();

    if (m_selectedItem && event->buttons() & Qt::LeftButton) {
        // Rect
        if (m_selectedItem->type() == 3) {
            QGraphicsRectItem *item = static_cast <QGraphicsRectItem *>(m_selectedItem);
            QRectF rect = item->rect().normalized();
            QPointF pos = item->pos();
            QPointF mousePosInRect = item->mapFromScene(mousePos);
            switch (m_resizeMode) {
            case TopLeft:
                if (mousePos.x() < pos.x() + rect.height() && mousePos.y() < pos.y() + rect.height()) {
                    item->setRect(rect.adjusted(0, 0, -mousePosInRect.x(), -mousePosInRect.y()));
                    item->setPos(mousePos);
                    m_modified = true;
                }
                break;
            case Top:
                if (mousePos.y() < pos.y() + rect.height()) {
                    rect.setBottom(rect.height() - mousePosInRect.y());
                    item->setRect(rect);
                    item->setPos(QPointF(pos.x(), mousePos.y()));
                    m_modified = true;
                }
                break;
            case TopRight:
                if (mousePos.x() > pos.x() && mousePos.y() < pos.y() + rect.height()) {
                    rect.setBottomRight(QPointF(mousePosInRect.x(), rect.bottom() - mousePosInRect.y()));
                    item->setRect(rect);
                    item->setPos(QPointF(pos.x(), mousePos.y()));
                    m_modified = true;
                }
                break;
            case Left:
                if (mousePos.x() < pos.x() + rect.width()) {
                    rect.setRight(rect.width() - mousePosInRect.x());
                    item->setRect(rect);
                    item->setPos(QPointF(mousePos.x(), pos.y()));
                    m_modified = true;
                }
                break;
            case Right:
                if (mousePos.x() > pos.x()) {
                    rect.setRight(mousePosInRect.x());
                    item->setRect(rect);
                    m_modified = true;
                }
                break;
            case BottomLeft:
                if (mousePos.x() < pos.x() + rect.width() && mousePos.y() > pos.y()) {
                    rect.setBottomRight(QPointF(rect.width() - mousePosInRect.x(), mousePosInRect.y()));
                    item->setRect(rect);
                    item->setPos(QPointF(mousePos.x(), pos.y()));
                    m_modified = true;
                }
                break;
            case Bottom:
                if (mousePos.y() > pos.y()) {
                    rect.setBottom(mousePosInRect.y());
                    item->setRect(rect);
                    m_modified = true;
                }
                break;
            case BottomRight:
                if (mousePos.x() > pos.x() && mousePos.y() > pos.y()) {
                    rect.setBottomRight(mousePosInRect);
                    item->setRect(rect);
                    m_modified = true;
                }
                break;
            default:
                QPointF diff = mousePos - m_clickPoint;
                m_clickPoint = mousePos;
                item->moveBy(diff.x(), diff.y());
                m_modified = true;
                break;
            }
        }
    } else {
        mousePos -= QPoint(4, 4);
        bool itemFound = false;
        QList<QGraphicsItem *> itemList = items(QRectF(mousePos, QSizeF(4, 4)).toRect());

        foreach(const QGraphicsItem* item, itemList) {
            if (item->zValue() >= 0 && item->flags() &QGraphicsItem::ItemIsMovable) {
                // Rect
                if (item->type() == 3) {
                    if (m_view == NULL)
                        continue;

                    itemFound = true;

                    switch (getResizeMode((QGraphicsRectItem*)item, m_view->mapFromScene(event->scenePos()))) {
                    case TopLeft:
                    case BottomRight:
                        m_view->setCursor(Qt::SizeFDiagCursor);
                        break;
                    case TopRight:
                    case BottomLeft:
                        m_view->setCursor(Qt::SizeBDiagCursor);
                        break;
                    case Top:
                    case Bottom:
                        m_view->setCursor(Qt::SizeVerCursor);
                        break;
                    case Left:
                    case Right:
                        m_view->setCursor(Qt::SizeHorCursor);
                        break;
                    default:
                        m_view->setCursor(Qt::OpenHandCursor);
                    }
                    break;
                }
            }
        }

        if (!itemFound && m_view)
            m_view->setCursor(Qt::ArrowCursor);

        QGraphicsScene::mouseMoveEvent(event);
    }
    if (m_modified && m_directUpdate) {
        emit actionFinished();
        m_modified = false;
    }
}

void MonitorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_enabled)
        return;

    QGraphicsScene::mouseReleaseEvent(event);
    if (m_modified) {
        m_modified = false;
        emit actionFinished();
    }
}

#include "monitorscene.moc"
