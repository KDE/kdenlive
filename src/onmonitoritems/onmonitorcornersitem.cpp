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

#include "onmonitorcornersitem.h"
#include "kdenlivesettings.h"

#include <algorithm>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QCursor>
#include <QGraphicsView>

OnMonitorCornersItem::OnMonitorCornersItem(QGraphicsItem *parent) :
    QGraphicsPolygonItem(parent)
    , m_mode(NoAction)
    , m_selectedCorner(-1)
    , m_modified(false)
    , m_view(nullptr)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);

    QPen framepen(Qt::SolidLine);
    framepen.setColor(Qt::yellow);
    setPen(framepen);
    setBrush(Qt::NoBrush);
    setAcceptHoverEvents(true);
}

OnMonitorCornersItem::cornersActions OnMonitorCornersItem::getMode(const QPointF &pos, int *corner)
{
    *corner = -1;
    if (polygon().count() != 4) {
        return NoAction;
    }

    QPainterPath mouseArea;
    qreal size = 12;
    if (getView()) {
        size /= m_view->matrix().m11();
    }
    mouseArea.addRect(pos.x() - size / 2, pos.y() - size / 2, size, size);
    for (int i = 0; i < 4; ++i) {
        if (mouseArea.contains(polygon().at(i))) {
            *corner = i;
            return Corner;
        }
    }
    if (KdenliveSettings::onmonitoreffects_cornersshowcontrols()) {
        if (mouseArea.contains(getCentroid())) {
            return Move;
        }

        for (int i = 0; i < 4; ++i) {
            int j = (i + 1) % 4;
            if (mouseArea.contains(QLineF(polygon().at(i), polygon().at(j)).pointAt(.5))) {
                *corner = i;
                return MoveSide;
            }
        }
    }

    return NoAction;
}

void OnMonitorCornersItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_mode = getMode(event->pos(), &m_selectedCorner);
    m_lastPoint = event->scenePos();

    if (m_mode == NoAction) {
        event->ignore();
    }
}

void OnMonitorCornersItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    /*if (event->buttons() != Qt::NoButton && (event->screenPos() - m_screenClickPoint).manhattanLength() < QApplication::startDragDistance()) {
     *   event->accept();
     *   return;
    }*/

    if (event->buttons() & Qt::LeftButton) {
        QPointF mousePos = mapFromScene(event->scenePos());
        QPolygonF p = polygon();
        switch (m_mode) {
        case Corner:
            p.replace(m_selectedCorner, mousePos);
            m_modified = true;
            break;
        case Move:
            p.translate(mousePos - m_lastPoint);
            m_modified = true;
            break;
        case MoveSide:
            p[m_selectedCorner] += mousePos - m_lastPoint;
            p[(m_selectedCorner + 1) % 4] += mousePos - m_lastPoint;
            m_modified = true;
            break;
        default:
            break;
        }
        m_lastPoint = mousePos;
        setPolygon(p);
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

void OnMonitorCornersItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_modified) {
        m_modified = false;
        emit changed();
    }
    event->accept();
}

void OnMonitorCornersItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    int corner;
    switch (getMode(event->pos(), &corner)) {
    case NoAction:
        unsetCursor();
        break;
    case Move:
        setCursor(QCursor(Qt::SizeAllCursor));
        break;
    default:
        setCursor(QCursor(Qt::OpenHandCursor));
        break;
    }
}

void OnMonitorCornersItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));

    if (KdenliveSettings::onmonitoreffects_cornersshowlines()) {
        QGraphicsPolygonItem::paint(painter, option, widget);
    }

    if (polygon().count() != 4) {
        return;
    }

    double baseSize = 1 / painter->worldTransform().m11();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(QBrush(isEnabled() ? Qt::yellow : Qt::red));
    double handleSize = 4  * baseSize;
    for (int i = 0; i < 4; ++i) {
        painter->drawEllipse(polygon().at(i), handleSize, handleSize);
    }

    if (KdenliveSettings::onmonitoreffects_cornersshowcontrols() && isEnabled()) {
        painter->setPen(QPen(Qt::red, 2, Qt::SolidLine));
        double toolSize = 6 * baseSize;
        // move tool
        QPointF c = getCentroid();
        painter->drawLine(QLineF(c - QPointF(toolSize, toolSize), c + QPointF(toolSize, toolSize)));
        painter->drawLine(QLineF(c - QPointF(-toolSize, toolSize), c + QPointF(-toolSize, toolSize)));

        // move side tools (2 corners at once)
        for (int i = 0; i < 4; ++i) {
            int j = (i + 1) % 4;
            QPointF m = QLineF(polygon().at(i), polygon().at(j)).pointAt(.5);
            painter->drawRect(QRectF(-toolSize / 2., -toolSize / 2., toolSize, toolSize).translated(m));
        }
    }
}

QPointF OnMonitorCornersItem::getCentroid()
{
    QList<QPointF> p = sortedClockwise();

    /*
     * See: http://local.wasp.uwa.edu.au/~pbourke/geometry/polyarea/
     */

    double A = 0;
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        A += p[j].x() * p[i].y() - p[i].x() * p[j].y();
    }
    A /= 2.;
    A = qAbs(A);

    double x = 0, y = 0;
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        double f = (p[i].x() * p[j].y() - p[j].x() * p[i].y());
        x += f * (p[i].x() + p[j].x());
        y += f * (p[i].y() + p[j].y());
    }
    x /= 6 * A;
    y /= 6 * A;
    return QPointF(x, y);
}

QList<QPointF> OnMonitorCornersItem::sortedClockwise()
{
    QList<QPointF> points = polygon().toList();
    QPointF &a = points[0];
    QPointF &b = points[1];
    QPointF &c = points[2];
    QPointF &d = points[3];

    /*
     * http://stackoverflow.com/questions/242404/sort-four-points-in-clockwise-order
     */

    double abc = a.x() * b.y() - a.y() * b.x() + b.x() * c.y() - b.y() * c.x() + c.x() * a.y() - c.y() * a.x();
    if (abc > 0) {
        double acd = a.x() * c.y() - a.y() * c.x() + c.x() * d.y() - c.y() * d.x() + d.x() * a.y() - d.y() * a.x();
        if (acd > 0) {
            ;
        } else {
            double abd = a.x() * b.y() - a.y() * b.x() + b.x() * d.y() - b.y() * d.x() + d.x() * a.y() - d.y() * a.x();
            if (abd > 0) {
                std::swap(d, c);
            } else {
                std::swap(a, d);
            }
        }
    } else {
        double acd = a.x() * c.y() - a.y() * c.x() + c.x() * d.y() - c.y() * d.x() + d.x() * a.y() - d.y() * a.x();
        if (acd > 0) {
            double abd = a.x() * b.y() - a.y() * b.x() + b.x() * d.y() - b.y() * d.x() + d.x() * a.y() - d.y() * a.x();
            if (abd > 0) {
                std::swap(b, c);
            } else {
                std::swap(a, b);
            }
        } else {
            std::swap(a, c);
        }
    }
    return points;
}

bool OnMonitorCornersItem::getView()
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

