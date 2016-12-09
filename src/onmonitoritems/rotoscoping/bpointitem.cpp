/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "bpointitem.h"
#include "splineitem.h"

#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsView>

BPointItem::BPointItem(const BPoint &point, QGraphicsItem *parent) :
    QAbstractGraphicsShapeItem(parent),
    m_selection(-1)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);

    setAcceptHoverEvents(true);

    setPos(point.p);
    m_point.h1 = mapFromScene(point.h1);
    m_point.p = mapFromScene(point.p);
    m_point.h2 = mapFromScene(point.h2);
    m_point.handlesLinked = false;

    m_view = scene()->views().first();
}

BPoint BPointItem::getPoint() const
{
    return BPoint(mapToScene(m_point.h1), mapToScene(m_point.p), mapToScene(m_point.h2));
}

void BPointItem::setPoint(const BPoint &point)
{
    setPos(point.p);
    prepareGeometryChange();
    m_point.h1 = mapFromScene(point.h1);
    m_point.p = mapFromScene(point.p);
    m_point.h2 = mapFromScene(point.h2);
}

int BPointItem::type() const
{
    return Type;
}

QRectF BPointItem::boundingRect() const
{
    QPolygonF p = QPolygonF() << m_point.h1 << m_point.p << m_point.h2;
    return p.boundingRect().adjusted(-6, -6, 6, 6);
}

void BPointItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (isEnabled()) {
        painter->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));
        painter->setBrush(QBrush(isSelected() ? Qt::red : Qt::yellow));
    } else {
        painter->setPen(QPen(Qt::gray, 1, Qt::SolidLine));
        painter->setBrush(QBrush(Qt::gray));
    }
    painter->setRenderHint(QPainter::Antialiasing);

    double handleSize = 6 / painter->worldTransform().m11();
    double handleSizeHalf = handleSize / 2;

    QPolygonF handle = QPolygonF() << QPointF(0, -handleSizeHalf) << QPointF(handleSizeHalf, 0) << QPointF(0, handleSizeHalf) << QPointF(-handleSizeHalf, 0);

    painter->drawLine(m_point.h1, m_point.p);
    painter->drawLine(m_point.p, m_point.h2);

    painter->drawConvexPolygon(handle.translated(m_point.h1.x(), m_point.h1.y()));
    painter->drawConvexPolygon(handle.translated(m_point.h2.x(), m_point.h2.y()));
    painter->drawEllipse(QRectF(m_point.p.x() - handleSizeHalf,
                                m_point.p.y() - handleSizeHalf, handleSize, handleSize));
}

int BPointItem::getSelection(const QPointF &pos)
{
    QList<qreal> d;
    d << QLineF(pos, m_point.h1).length() << QLineF(pos, m_point.p).length() << QLineF(pos, m_point.h2).length();
    // index of point nearest to pos
    int i = (d[1] < d[0] && d[1] < d[2]) ? 1 : (d[0] < d[2] ? 0 : 2);

    if (d[i] < 6 / m_view->matrix().m11()) {
        return i;
    }

    return -1;
}

void BPointItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_selection = getSelection(event->pos());

    if (m_selection < 0) {
        event->ignore();
        setSelected(false);
    } else {
        if (event->button() == Qt::RightButton && m_selection == 1) {
            SplineItem *parent = qgraphicsitem_cast<SplineItem *>(parentItem());
            if (parent) {
                parent->removeChild(this);
                return;
            }
        }
        setSelected(true);
    }
}

void BPointItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    prepareGeometryChange();
    switch (m_selection) {
    case 0:
        m_point.setH1(event->pos());
        break;
    case 1:
        m_point.setP(event->pos());
        break;
    case 2:
        m_point.setH2(event->pos());
        break;
    }

    if (parentItem()) {
        SplineItem *parent = qgraphicsitem_cast<SplineItem *>(parentItem());
        if (parent) {
            parent->updateSpline(true);
        }
    }
}

void BPointItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (parentItem()) {
        SplineItem *parent = qgraphicsitem_cast<SplineItem *>(parentItem());
        if (parent) {
            parent->updateSpline(false);
        }
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

void BPointItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (getSelection(event->pos()) < 0) {
        unsetCursor();
    } else {
        setCursor(QCursor(Qt::PointingHandCursor));
    }
}
