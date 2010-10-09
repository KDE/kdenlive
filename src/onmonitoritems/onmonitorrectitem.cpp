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

#include "onmonitorrectitem.h"
#include "kdenlivesettings.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QCursor>

OnMonitorRectItem::OnMonitorRectItem(const QRectF &rect, QGraphicsItem* parent) :
        QGraphicsRectItem(rect, parent)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);

    QPen framepen(Qt::SolidLine);
    framepen.setColor(Qt::yellow);
    setPen(framepen);
    setBrush(Qt::transparent);
}

rectActions OnMonitorRectItem::getMode(QPoint pos)
{
    pos = mapFromScene(pos).toPoint();
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
        return NoAction;
}

int OnMonitorRectItem::type() const
{
    return Type;
}

void OnMonitorRectItem::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void OnMonitorRectItem::slotMousePressed(QGraphicsSceneMouseEvent* event)
{
    if (!m_enabled)
        return;

    m_clickPoint = event->scenePos();
    m_mode = getMode(m_clickPoint.toPoint());
}

void OnMonitorRectItem::slotMouseReleased(QGraphicsSceneMouseEvent* event)
{
    if (m_modified) {
        m_modified = false;
        emit actionFinished();
    }

    event->accept();
}

void OnMonitorRectItem::slotMouseMoved(QGraphicsSceneMouseEvent* event)
{
    if (!m_enabled) {
        emit setCursor(QCursor(Qt::ArrowCursor));
        return;
    }

    /*if (event->buttons() != Qt::NoButton && (event->screenPos() - m_screenClickPoint).manhattanLength() < QApplication::startDragDistance()) {
     *   event->accept();
     *   return;
    }*/

    QPointF mousePos = event->scenePos();

    if (event->buttons() & Qt::LeftButton) {
        QRectF r = rect().normalized();
        QPointF p = pos();
        QPointF mousePosInRect = mapFromScene(mousePos);
        switch (m_mode) {
        case ResizeTopLeft:
            if (mousePos.x() < p.x() + r.height() && mousePos.y() < p.y() + r.height()) {
                setRect(r.adjusted(0, 0, -mousePosInRect.x(), -mousePosInRect.y()));
                setPos(mousePos);
                m_modified = true;
            }
            break;
        case ResizeTop:
            if (mousePos.y() < p.y() + r.height()) {
                r.setBottom(r.height() - mousePosInRect.y());
                setRect(r);
                setPos(QPointF(p.x(), mousePos.y()));
                m_modified = true;
            }
            break;
        case ResizeTopRight:
            if (mousePos.x() > p.x() && mousePos.y() < p.y() + r.height()) {
                r.setBottomRight(QPointF(mousePosInRect.x(), r.bottom() - mousePosInRect.y()));
                setRect(r);
                setPos(QPointF(p.x(), mousePos.y()));
                m_modified = true;
            }
            break;
        case ResizeLeft:
            if (mousePos.x() < p.x() + r.width()) {
                r.setRight(r.width() - mousePosInRect.x());
                setRect(r);
                setPos(QPointF(mousePos.x(), p.y()));
                m_modified = true;
            }
            break;
        case ResizeRight:
            if (mousePos.x() > p.x()) {
                r.setRight(mousePosInRect.x());
                setRect(r);
                m_modified = true;
            }
            break;
        case ResizeBottomLeft:
            if (mousePos.x() < p.x() + r.width() && mousePos.y() > p.y()) {
                r.setBottomRight(QPointF(r.width() - mousePosInRect.x(), mousePosInRect.y()));
                setRect(r);
                setPos(QPointF(mousePos.x(), p.y()));
                m_modified = true;
            }
            break;
        case ResizeBottom:
            if (mousePos.y() > p.y()) {
                r.setBottom(mousePosInRect.y());
                setRect(r);
                m_modified = true;
            }
            break;
        case ResizeBottomRight:
            if (mousePos.x() > p.x() && mousePos.y() > p.y()) {
                r.setBottomRight(mousePosInRect);
                setRect(r);
                m_modified = true;
            }
            break;
        case Move:
            QPointF diff = mousePos - m_clickPoint;
            m_clickPoint = mousePos;
            moveBy(diff.x(), diff.y());
            m_modified = true;
            break;
        }
    } else {
        switch (getMode(event->scenePos().toPoint())) {
        case ResizeTopLeft:
        case ResizeBottomRight:
            emit setCursor(QCursor(Qt::SizeFDiagCursor));
            break;
        case ResizeTopRight:
        case ResizeBottomLeft:
            emit setCursor(QCursor(Qt::SizeBDiagCursor));
            break;
        case ResizeTop:
        case ResizeBottom:
            emit setCursor(QCursor(Qt::SizeVerCursor));
            break;
        case ResizeLeft:
        case ResizeRight:
            emit setCursor(QCursor(Qt::SizeHorCursor));
            break;
        case Move:
            emit setCursor(QCursor(Qt::OpenHandCursor));
            break;
        default:
            emit setCursor(QCursor(Qt::ArrowCursor));
            break;
        }
    }
    if (m_modified && KdenliveSettings::monitorscene_directupdate()) {
        emit actionFinished();
        m_modified = false;
    }

    event->accept();
}

void OnMonitorRectItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    QGraphicsRectItem::paint(painter, option, widget);

    if (m_enabled) {
        double handleSize = 6 / painter->matrix().m11();
        double halfHandleSize = handleSize / 2;
        painter->fillRect(-halfHandleSize, -halfHandleSize, handleSize, handleSize, QColor(Qt::yellow));
        painter->fillRect(option->rect.width() - halfHandleSize, -halfHandleSize, handleSize, handleSize, QColor(Qt::yellow));
        painter->fillRect(option->rect.width() - halfHandleSize, option->rect.height() - halfHandleSize, handleSize, handleSize, QColor(Qt::yellow));
        painter->fillRect(-halfHandleSize, option->rect.height() - halfHandleSize, handleSize, handleSize, QColor(Qt::yellow));
    }
}



#include "onmonitorrectitem.moc"
