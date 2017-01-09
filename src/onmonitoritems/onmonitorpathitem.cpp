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

#include "onmonitorpathitem.h"
#include "kdenlivesettings.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QCursor>
#include <QGraphicsView>
#include <QApplication>

#include <mlt++/Mlt.h>

OnMonitorPathItem::OnMonitorPathItem(QGraphicsItem *parent) :
    QGraphicsPathItem(parent),
    m_modified(false),
    m_view(nullptr),
    m_activePoint(-1)
{
    setFlags(QGraphicsItem::ItemIsMovable);

    QPen framepen(Qt::SolidLine);
    framepen.setColor(Qt::red);
    setPen(framepen);
    setBrush(Qt::transparent);
    setAcceptHoverEvents(true);
}

void OnMonitorPathItem::setPoints(Mlt::Geometry *geometry)
{
    m_points.clear();
    QRectF r;
    int pos = 0;
    Mlt::GeometryItem item;
    while (!geometry->next_key(&item, pos)) {
        r = QRectF(item.x(), item.y(), item.w(), item.h());
        m_points << r.center();
        pos = item.frame() + 1;
    }
    rebuildShape();
}

void OnMonitorPathItem::rebuildShape()
{
    if (m_activePoint > m_points.count()) {
        m_activePoint = -1;
    }
    QPainterPath p;
    QPainterPath shape;

    if (!m_points.isEmpty()) {
        QRectF r(0, 0, 20, 20);
        r.moveCenter(m_points.at(0));
        shape.addRect(r);

        p.moveTo(m_points.at(0));
        for (int i = 1; i < m_points.count(); ++i) {
            p.lineTo(m_points.at(i));
            r.moveCenter(m_points.at(i));
            shape.addRect(r);
        }
    }
    prepareGeometryChange();
    m_shape = shape;
    setPath(p);
}

void OnMonitorPathItem::getMode(const QPointF &pos)
{
    double dist  = 8;
    if (getView()) {
        dist /= m_view->matrix().m11();
    }
    // Item mapped coordinates
    for (int i = 0; i < m_points.count(); ++i) {
        if ((pos - m_points.at(i)).manhattanLength() <= dist) {
            m_activePoint = i;
            return;
        }
    }
    m_activePoint = -1;
}

void OnMonitorPathItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    /*if (event->buttons() != Qt::NoButton && (event->screenPos() - m_screenClickPoint).manhattanLength() < QApplication::startDragDistance()) {
     *   event->accept();
     *   return;
    }*/

    if (m_activePoint >= 0 && event->buttons() & Qt::LeftButton) {
        QPointF mousePos = event->pos();
        m_points[m_activePoint] = mousePos;
        rebuildShape();
        m_modified = true;
        update();
    }

    if (m_modified) {
        event->accept();
        if (KdenliveSettings::monitorscene_directupdate()) {
            emit changed();
            m_modified = false;
        }
    } else {
        event->ignore();
    }
}

QList<QPointF> OnMonitorPathItem::points() const
{
    return m_points;
}

void OnMonitorPathItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_modified) {
        m_modified = false;
        emit changed();
    }
    event->accept();
}

QRectF OnMonitorPathItem::boundingRect() const
{
    return shape().boundingRect();
}

QPainterPath OnMonitorPathItem::shape() const
{
    return m_shape;
}

void OnMonitorPathItem::hoverLeaveEvent(QGraphicsSceneHoverEvent * /*event*/)
{
    if (m_activePoint != -1) {
        m_activePoint = -1;
        update();
    }
    unsetCursor();
}

void OnMonitorPathItem::hoverEnterEvent(QGraphicsSceneHoverEvent * /*event*/)
{
    setCursor(QCursor(Qt::PointingHandCursor));
}

void OnMonitorPathItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    int currentPoint = m_activePoint;
    getMode(event->pos());
    if (currentPoint != m_activePoint) {
        update();
    }
    setCursor(QCursor(Qt::PointingHandCursor));
}

void OnMonitorPathItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    //Q_UNUSED(widget)
    QGraphicsPathItem::paint(painter, option, widget);

    double w = 6;
    double h = 6;
    if (getView()) {
        w /= m_view->matrix().m11();
        h /= m_view->matrix().m22();
    }

    QRectF handle(0, 0, w, h);
    for (int i = 0; i < m_points.count(); ++i) {
        handle.moveCenter(m_points.at(i));
        painter->fillRect(handle, m_activePoint == i ? Qt::blue : pen().color());
    }
}

bool OnMonitorPathItem::getView()
{
    if (m_view) {
        return true;
    }

    if (scene() && !scene()->views().isEmpty()) {
        m_view = scene()->views().first();
        return true;
    } else {
        return false;
    }
}

