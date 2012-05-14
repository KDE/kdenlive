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
#include "onmonitoritems/rotoscoping/bpointitem.h"
#include "onmonitoritems/rotoscoping/splineitem.h"
#include "kdenlivesettings.h"

#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <QScrollBar>


MonitorScene::MonitorScene(Render *renderer, QObject* parent) :
        QGraphicsScene(parent),
        m_renderer(renderer),
        m_view(NULL),
        m_backgroundImage(QImage()),
        m_enabled(true),
        m_zoom(1.0),
        m_groupMove(false)
{
    setBackgroundBrush(QBrush(QColor(KdenliveSettings::window_background().name())));

    QPen framepen(Qt::DotLine);
    framepen.setColor(Qt::red);

    m_frameBorder = new QGraphicsRectItem(QRectF(0, 0, m_renderer->frameRenderWidth(), m_renderer->renderHeight()));
    m_frameBorder->setPen(framepen);
    m_frameBorder->setZValue(-1);
    m_frameBorder->setBrush(Qt::transparent);
    m_frameBorder->setFlags(0);
    addItem(m_frameBorder);

    m_lastUpdate = QTime::currentTime();
    m_background = new QGraphicsPixmapItem();
    m_background->setZValue(-2);
    m_background->setFlags(0);
    m_background->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    m_background->setTransformationMode(Qt::FastTransformation);
    QPixmap bg(m_renderer->frameRenderWidth(), m_renderer->renderHeight());
    bg.fill();
    m_background->setPixmap(bg);
    addItem(m_background);

    connect(m_renderer, SIGNAL(frameUpdated(QImage)), this, SLOT(slotSetBackgroundImage(QImage)));
}

void MonitorScene::centerView()
{
    if (m_view) m_view->centerOn(m_frameBorder);
}

void MonitorScene::cleanup()
{
    // Reset scene rect
    setSceneRect(m_frameBorder->boundingRect());
}

void MonitorScene::setUp()
{
    if (views().count() > 0) {
        m_view = views().at(0);
        m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    } else {
        m_view = NULL;
    }
}

void MonitorScene::resetProfile()
{
    const QRectF border(0, 0, m_renderer->frameRenderWidth(), m_renderer->renderHeight());
    m_frameBorder->setRect(border);
}

void MonitorScene::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void MonitorScene::slotUpdateBackground()
{
    if (m_view && m_view->isVisible()) {
        if (m_lastUpdate.msecsTo(QTime::currentTime()) > 50) {
            m_background->setPixmap(QPixmap::fromImage(m_backgroundImage));
            m_lastUpdate = QTime::currentTime();
        }
    }
}

void MonitorScene::slotSetBackgroundImage(const QImage &image)
{
    if (m_view && m_view->isVisible()) {
        m_backgroundImage = image;
        slotUpdateBackground();
    }
}

void MonitorScene::slotZoom(int value)
{
    if (m_view) {
        m_zoom = value / 100.0;
        m_view->resetTransform();
        m_view->scale(m_renderer->renderWidth() * m_zoom / m_renderer->frameRenderWidth(), m_zoom);
        emit zoomChanged(value);
    }
}

void MonitorScene::slotZoomFit()
{
    if (m_view) {
        int xzoom = 100 * m_view->viewport()->height() / m_renderer->renderHeight();
        int yzoom = 100 * m_view->viewport()->width() / m_renderer->renderWidth();
        slotZoom(qMin(xzoom, yzoom));
        m_view->centerOn(m_frameBorder);
    }
}

void MonitorScene::slotZoomOriginal()
{
    slotZoom(100);
    if (m_view)
        m_view->centerOn(m_frameBorder);
}

void MonitorScene::slotZoomOut(int by)
{
    slotZoom(qMax(0, (int)(m_zoom * 100 - by)));
}

void MonitorScene::slotZoomIn(int by)
{
    slotZoom(qMin(300, (int)(m_zoom * 100 + by + 0.5)));
}

void MonitorScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QList <QGraphicsItem *> selected = selectedItems();

    QGraphicsScene::mousePressEvent(event);

    if (selected.count() < selectedItems().count()) {
        // mouse click on item not in selection group
        // -> select only this item
        foreach (QGraphicsItem *item, selected) {
            if (item)
                item->setSelected(false);
        }
    }

    if (event->isAccepted() && selectedItems().count() > 1) {
        // multiple items selected + mouse pressed on an item
        selected = selectedItems();
        foreach (QGraphicsItem *item, selected) {
            if (qgraphicsitem_cast<BPointItem*>(item)) {
                // works with rotoscoping only for now
                m_groupMove = true;
                m_lastPos = event->scenePos();
                return;
            }
        }
    }

    if (!event->isAccepted() && event->buttons() & Qt::LeftButton) {
        if (event->modifiers() == Qt::ControlModifier)
            m_view->setDragMode(QGraphicsView::ScrollHandDrag);
        else if (event->modifiers() == Qt::ShiftModifier)
            m_view->setDragMode(QGraphicsView::RubberBandDrag);
    }
}

void MonitorScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_groupMove) {
        // we want to move multiple items
        // rotoscoping only for now
        QPointF diff =  event->scenePos() - m_lastPos;
        if (diff != QPointF(0, 0)) {
            m_lastPos = event->scenePos();
            QList <QGraphicsItem *> selected = selectedItems();
            int first = -1;
            int i = 0;
            foreach (QGraphicsItem *item, selected) {
                BPointItem *bpoint = qgraphicsitem_cast<BPointItem *>(item);
                if (bpoint) {
                    if (first < 0)
                        first = i;
                    BPoint p = bpoint->getPoint();
                    p.setP(p.p + diff);
                    bpoint->setPoint(p);
                }
                ++i;
            }

            if (first >= 0) {
                QGraphicsItem *item = selected.at(first);
                if (item->parentItem()) {
                    SplineItem *parent = qgraphicsitem_cast<SplineItem*>(item->parentItem());
                    if (parent)
                        parent->updateSpline(true);
                }
            }
        }
    } else {
        QGraphicsScene::mouseMoveEvent(event);
    }
}

void MonitorScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_groupMove) {
        QList <QGraphicsItem *> selected = selectedItems();
        foreach (QGraphicsItem *item, selected) {
            if (qgraphicsitem_cast<BPointItem*>(item) && item->parentItem()) {
                SplineItem *parent = qgraphicsitem_cast<SplineItem*>(item->parentItem());
                if (parent) {
                    parent->updateSpline(false);
                    break;
                }
            }
        }
        m_groupMove = false;
    }
    QGraphicsScene::mouseReleaseEvent(event);
    m_view->setDragMode(QGraphicsView::NoDrag);
}
void MonitorScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)

    if (!m_enabled)
        emit addKeyframe();
}

void MonitorScene::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    if (event->modifiers() == Qt::ControlModifier) {
        if (event->delta() > 0) {
            m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            slotZoomIn(5);
            m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        } else {
            slotZoomOut(5);
        }
    } else {
        QAbstractSlider::SliderAction action;
        if (event->delta() > 0)
            action = QAbstractSlider::SliderSingleStepSub;
        else
            action = QAbstractSlider::SliderSingleStepAdd;
        if (event->orientation() == Qt::Horizontal)
            m_view->horizontalScrollBar()->triggerAction(action);
        else
            m_view->verticalScrollBar()->triggerAction(action);
    }

    event->accept();
}

#include "monitorscene.moc"
