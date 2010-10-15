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
    setBrush(Qt::NoBrush);
}

cornersActions OnMonitorCornersItem::getMode(QPoint pos)
{
    QPainterPath mouseArea;
    pos = mapFromScene(pos).toPoint();
    mouseArea.addRect(pos.x() - 6, pos.y() - 6, 12, 12);
    if (mouseArea.contains(polygon().at(0)))
        return Corner1;
    else if (mouseArea.contains(polygon().at(1)))
        return Corner2;
    else if (mouseArea.contains(polygon().at(2)))
        return Corner3;
    else if (mouseArea.contains(polygon().at(3)))
        return Corner4;
    else
        return NoAction;
}

void OnMonitorCornersItem::slotMousePressed(QGraphicsSceneMouseEvent* event)
{
    event->accept();

    if (!isEnabled())
        return;

    m_mode = getMode(event->scenePos().toPoint());
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
        QPoint mousePos = mapFromScene(event->scenePos()).toPoint();
        QPolygon p = polygon().toPolygon();
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
        default:
            break;
        }
        setPolygon(p);
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

void OnMonitorCornersItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    QGraphicsPolygonItem::paint(painter, option, widget);

    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(QBrush(Qt::yellow));
    double handleSize = 4 / painter->matrix().m11();
    painter->drawEllipse(polygon().at(0), handleSize, handleSize);
    painter->drawEllipse(polygon().at(1), handleSize, handleSize);
    painter->drawEllipse(polygon().at(2), handleSize, handleSize);
    painter->drawEllipse(polygon().at(3), handleSize, handleSize);
}

#include "onmonitorcornersitem.moc"
