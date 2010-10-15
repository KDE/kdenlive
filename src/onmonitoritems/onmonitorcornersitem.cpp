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
    setBrush(Qt::transparent);
}

cornersActions OnMonitorCornersItem::getMode(QPoint pos)
{
    /*pos = mapFromScene(pos).toPoint();
    // Item mapped coordinates
    QPolygon pol(rect().normalized().toRect());

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
        return ResizeTopLeft;
    else if (mouseArea.contains(pol.point(2)))
        return ResizeBottomRight;
    else if (mouseArea.contains(pol.point(1)))
        return ResizeTopRight;
    else if (mouseArea.contains(pol.point(3)))
        return ResizeBottomLeft;
    else if (top.intersects(mouseArea))
        return ResizeTop;
    else if (bottom.intersects(mouseArea))
        return ResizeBottom;
    else if (right.intersects(mouseArea))
        return ResizeRight;
    else if (left.intersects(mouseArea))
        return ResizeLeft;
    else if (rect().normalized().contains(pos))
        return Move;
    else
        return NoAction;*/
    return NoAction;
}

void OnMonitorCornersItem::slotMousePressed(QGraphicsSceneMouseEvent* event)
{
    event->accept();

    if (!isEnabled())
        return;

    m_clickPoint = event->scenePos();
    m_mode = getMode(m_clickPoint.toPoint());
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

    QPointF mousePos = event->scenePos();

    if (event->buttons() & Qt::LeftButton) {
        m_clickPoint = mousePos;
        switch (m_mode) {
        case Corner1:
            
            break;
        case Corner2:
            
            break;
        case Corner3:
            
            break;
        case Corner4:
            
            break;
        default:
            break;
        }
    } else {
        switch (getMode(event->scenePos().toPoint())) {
        case NoAction:
            emit requestCursor(QCursor(Qt::ArrowCursor));
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

/*void OnMonitorRectItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(widget);

    painter->setPen(pen());
    painter->drawRect(option->rect);

    if (isEnabled()) {
        double handleSize = 6 / painter->matrix().m11();
        double halfHandleSize = handleSize / 2;
        painter->fillRect(-halfHandleSize, -halfHandleSize, handleSize, handleSize, QColor(Qt::yellow));
        painter->fillRect(option->rect.width() - halfHandleSize, -halfHandleSize, handleSize, handleSize, QColor(Qt::yellow));
        painter->fillRect(option->rect.width() - halfHandleSize, option->rect.height() - halfHandleSize, handleSize, handleSize, QColor(Qt::yellow));
        painter->fillRect(-halfHandleSize, option->rect.height() - halfHandleSize, handleSize, handleSize, QColor(Qt::yellow));
    }
}*/

#include "onmonitorcornersitem.moc"
