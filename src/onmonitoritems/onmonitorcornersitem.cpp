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

OnMonitorCornersItem::OnMonitorCornersItem(MonitorScene* scene, QGraphicsItem* parent) :
        AbstractOnMonitorItem(scene),
        QGraphicsPolygonItem(parent)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);

    QPen framepen(Qt::SolidLine);
    framepen.setColor(Qt::yellow);
    setPen(framepen);
    setBrush(Qt::NoBrush);
}

OnMonitorCornersItem::cornersActions OnMonitorCornersItem::getMode(QPointF pos)
{
    QPainterPath mouseArea;
    pos = mapFromScene(pos);
    mouseArea.addRect(pos.x() - 6, pos.y() - 6, 12, 12);
    if (mouseArea.contains(polygon().at(0)))
        return Corner1;
    else if (mouseArea.contains(polygon().at(1)))
        return Corner2;
    else if (mouseArea.contains(polygon().at(2)))
        return Corner3;
    else if (mouseArea.contains(polygon().at(3)))
        return Corner4;
    else if (KdenliveSettings::onmonitoreffects_cornersshowcontrols() && mouseArea.contains(getCentroid()))
        return Move;
    else
        return NoAction;
}

void OnMonitorCornersItem::slotMousePressed(QGraphicsSceneMouseEvent* event)
{
    event->accept();

    if (!isEnabled())
        return;

    m_mode = getMode(event->scenePos());
    m_lastPoint = event->scenePos();
}

void OnMonitorCornersItem::slotMouseMoved(QGraphicsSceneMouseEvent* event)
{
    event->accept();

    if (!isEnabled()) {
        emit requestCursor(QCursor(Qt::ArrowCursor));
        return;
    }

    /*if (event->buttons() != Qt::NoButton && (event->screenPos() - m_screenClickPoint).manhattanLength() < QApplication::startDragDistance()) {
     *   event->accept();
     *   return;
    }*/

    if (event->buttons() & Qt::LeftButton) {
        QPointF mousePos = mapFromScene(event->scenePos());
        QPolygonF p = polygon();
        switch (m_mode) {
        case Corner1:
            p.replace(0, mousePos);
            m_modified = true;
            break;
        case Corner2:
            p.replace(1, mousePos);
            m_modified = true;
            break;
        case Corner3:
            p.replace(2, mousePos);
            m_modified = true;
            break;
        case Corner4:
            p.replace(3, mousePos);
            m_modified = true;
            break;
        case Move:
            p.translate(mousePos - m_lastPoint);
            m_modified = true;
            break;
        default:
            break;
        }
        m_lastPoint = mousePos;
        setPolygon(p);
    } else {
        switch (getMode(event->scenePos())) {
        case NoAction:
            emit requestCursor(QCursor(Qt::ArrowCursor));
            break;
        case Move:
            emit requestCursor(QCursor(Qt::SizeAllCursor));
            break;
        default:
            emit requestCursor(QCursor(Qt::OpenHandCursor));
            break;
        }
    }
    if (m_modified && KdenliveSettings::monitorscene_directupdate()) {
        emit actionFinished();
        m_modified = false;
    }
}

void OnMonitorCornersItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    painter->setPen(QPen(Qt::yellow, 1, Qt::SolidLine));

    if (KdenliveSettings::onmonitoreffects_cornersshowlines())
        QGraphicsPolygonItem::paint(painter, option, widget);

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(QBrush(Qt::yellow));
    double handleSize = 4 / painter->matrix().m11();
    painter->drawEllipse(polygon().at(0), handleSize, handleSize);
    painter->drawEllipse(polygon().at(1), handleSize, handleSize);
    painter->drawEllipse(polygon().at(2), handleSize, handleSize);
    painter->drawEllipse(polygon().at(3), handleSize, handleSize);

    if (KdenliveSettings::onmonitoreffects_cornersshowcontrols()) {
        painter->setPen(QPen(Qt::red, 2, Qt::SolidLine));
        QPointF c = getCentroid();
        handleSize *= 1.5;
        painter->drawLine(QLineF(c - QPointF(handleSize, handleSize), c + QPointF(handleSize, handleSize)));
        painter->drawLine(QLineF(c - QPointF(-handleSize, handleSize), c + QPointF(-handleSize, handleSize)));
    }
}

QPointF OnMonitorCornersItem::getCentroid()
{
    QList <QPointF> p = sortedClockwise();

    /*
     * See: http://local.wasp.uwa.edu.au/~pbourke/geometry/polyarea/
     */

    double A = 0;
    int i, j;
    for (i = 0; i < 4; ++i) {
        j = (i + 1) % 4;
        A += p[j].x() * p[i].y() - p[i].x() * p[j].y();
    }
    A /= 2.;
    A = qAbs(A);

    double x = 0, y = 0, f;
    for (i = 0; i < 4; ++i) {
        j = (i + 1) % 4;
        f = (p[i].x() * p[j].y() - p[j].x() * p[i].y());
        x += f * (p[i].x() + p[j].x());
        y += f * (p[i].y() + p[j].y());
    }
    x /= 6*A;
    y /= 6*A;
    return QPointF(x, y);
}

QList <QPointF> OnMonitorCornersItem::sortedClockwise()
{
    QList <QPointF> points = polygon().toList();
    QPointF& a = points[0];
    QPointF& b = points[1];
    QPointF& c = points[2];
    QPointF& d = points[3];

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
            } else{
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

#include "onmonitorcornersitem.moc"
