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
#include <QGraphicsView>

OnMonitorRectItem::OnMonitorRectItem(const QRectF &rect, double dar, QGraphicsItem *parent) :
    QGraphicsRectItem(rect, parent)
    , m_dar(dar)
    , m_mode(NoAction)
    , m_modified(false)
    , m_view(nullptr)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);

    QPen framepen(Qt::SolidLine);
    framepen.setColor(Qt::yellow);
    setPen(framepen);
    setBrush(Qt::transparent);
    setAcceptHoverEvents(true);
}

rectActions OnMonitorRectItem::getMode(const QPointF &pos)
{
    // Item mapped coordinates
    QPolygonF pol(rect().normalized());

    QPainterPath top(pol.at(0));
    top.lineTo(pol.at(1));
    QPainterPath bottom(pol.at(2));
    bottom.lineTo(pol.at(3));
    QPainterPath left(pol.at(0));
    left.lineTo(pol.at(3));
    QPainterPath right(pol.at(1));
    right.lineTo(pol.at(2));

    QPainterPath mouseArea;
    qreal xsize = 12;
    qreal ysize = 12;
    if (getView()) {
        xsize /= m_view->matrix().m11();
        ysize /= m_view->matrix().m22();
    }
    mouseArea.addRect(pos.x() - xsize / 2, pos.y() - ysize / 2, xsize, ysize);

    // Check for collisions between the mouse and the borders
    if (mouseArea.contains(pol.at(0))) {
        return ResizeTopLeft;
    } else if (mouseArea.contains(pol.at(2))) {
        return ResizeBottomRight;
    } else if (mouseArea.contains(pol.at(1))) {
        return ResizeTopRight;
    } else if (mouseArea.contains(pol.at(3))) {
        return ResizeBottomLeft;
    } else if (top.intersects(mouseArea)) {
        return ResizeTop;
    } else if (bottom.intersects(mouseArea)) {
        return ResizeBottom;
    } else if (right.intersects(mouseArea)) {
        return ResizeRight;
    } else if (left.intersects(mouseArea)) {
        return ResizeLeft;
    } else if (rect().normalized().contains(pos)) {
        return Move;
    } else {
        return NoAction;
    }
}

void OnMonitorRectItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    event->accept();

    m_lastPoint = event->scenePos();
    m_oldRect = rect().normalized();
    m_mode = getMode(event->pos());
}

void OnMonitorRectItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    /*if (event->buttons() != Qt::NoButton && (event->screenPos() - m_screenClickPoint).manhattanLength() < QApplication::startDragDistance()) {
     *   event->accept();
     *   return;
    }*/

    if (event->buttons() & Qt::LeftButton) {
        QRectF r = rect().normalized();
        QPointF p = pos();
        QPointF mousePos = event->scenePos();
        QPointF mousePosInRect = event->pos();
        QPointF diff = mousePos - m_lastPoint;
        m_lastPoint = mousePos;

        switch (m_mode) {
        case ResizeTopLeft:
            if (mousePos.x() < p.x() + r.height() && mousePos.y() < p.y() + r.height()) {
                r.adjust(0, 0, -mousePosInRect.x(), -mousePosInRect.y());
                setPos(mousePos);
                m_modified = true;
            }
            break;
        case ResizeTop:
            if (mousePos.y() < p.y() + r.height()) {
                r.setBottom(r.height() - mousePosInRect.y());
                setPos(QPointF(p.x(), mousePos.y()));
                m_modified = true;
            }
            break;
        case ResizeTopRight:
            if (mousePos.x() > p.x() && mousePos.y() < p.y() + r.height()) {
                r.setBottomRight(QPointF(mousePosInRect.x(), r.bottom() - mousePosInRect.y()));
                setPos(QPointF(p.x(), mousePos.y()));
                m_modified = true;
            }
            break;
        case ResizeLeft:
            if (mousePos.x() < p.x() + r.width()) {
                r.setRight(r.width() - mousePosInRect.x());
                setPos(QPointF(mousePos.x(), p.y()));
                m_modified = true;
            }
            break;
        case ResizeRight:
            if (mousePos.x() > p.x()) {
                r.setRight(mousePosInRect.x());
                m_modified = true;
            }
            break;
        case ResizeBottomLeft:
            if (mousePos.x() < p.x() + r.width() && mousePos.y() > p.y()) {
                r.setBottomRight(QPointF(r.width() - mousePosInRect.x(), mousePosInRect.y()));
                setPos(QPointF(mousePos.x(), p.y()));
                m_modified = true;
            }
            break;
        case ResizeBottom:
            if (mousePos.y() > p.y()) {
                r.setBottom(mousePosInRect.y());
                m_modified = true;
            }
            break;
        case ResizeBottomRight:
            if (mousePos.x() > p.x() && mousePos.y() > p.y()) {
                r.setBottomRight(mousePosInRect);
                m_modified = true;
            }
            break;
        case Move:
            moveBy(diff.x(), diff.y());
            m_modified = true;
            break;
        default:
            break;
        }

        // Keep aspect ratio
        if (event->modifiers() == Qt::ControlModifier) {
            // compare to rect during mouse press:
            // if we subtract rect() we'll get a whole lot of flickering
            // because of diffWidth > diffHeight changing all the time
            int diffWidth = qAbs(r.width() - m_oldRect.width());
            int diffHeight = qAbs(r.height() - m_oldRect.height());

            if (diffHeight != 0 || diffWidth != 0) {
                if (diffWidth > diffHeight) {
                    r.setBottom(r.width() / m_dar);
                } else {
                    r.setRight(r.height() * m_dar);
                }

                // the rect's position changed
                if (p - pos() != QPointF()) {
                    if (diffWidth > diffHeight) {
                        if (m_mode != ResizeBottomLeft) {
                            setY(p.y() - r.height() + rect().normalized().height());
                        }
                    } else {
                        if (m_mode != ResizeTopRight) {
                            setX(p.x() - r.width() + rect().normalized().width());
                        }
                    }
                }
            }
        }

        setRect(r);
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

void OnMonitorRectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_modified) {
        m_modified = false;
        emit changed();
    }
    event->accept();
}

void OnMonitorRectItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    switch (getMode(event->pos())) {
    case ResizeTopLeft:
    case ResizeBottomRight:
        setCursor(QCursor(Qt::SizeFDiagCursor));
        break;
    case ResizeTopRight:
    case ResizeBottomLeft:
        setCursor(QCursor(Qt::SizeBDiagCursor));
        break;
    case ResizeTop:
    case ResizeBottom:
        setCursor(QCursor(Qt::SizeVerCursor));
        break;
    case ResizeLeft:
    case ResizeRight:
        setCursor(QCursor(Qt::SizeHorCursor));
        break;
    case Move:
        setCursor(QCursor(Qt::OpenHandCursor));
        break;
    default:
        unsetCursor();
        break;
    }
}

void OnMonitorRectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)
    Q_UNUSED(option)

    painter->setPen(pen());
    //painter->setClipRect(option->rect);
    const QRectF r = rect();
    painter->drawRect(r);
    QRectF handle = painter->worldTransform().inverted().mapRect(QRectF(0, 0, 6, 6));
    if (isEnabled()) {
        handle.moveTopLeft(r.topLeft());
        painter->fillRect(handle, QColor(Qt::yellow));
        handle.moveTopRight(r.topRight());
        painter->fillRect(handle, QColor(Qt::yellow));
        handle.moveBottomLeft(r.bottomLeft());
        painter->fillRect(handle, QColor(Qt::yellow));
        handle.moveBottomRight(r.bottomRight());
        painter->fillRect(handle, QColor(Qt::yellow));
    }

    // Draw cross at center
    QPointF center = r.center();
    painter->drawLine(center + QPointF(-handle.width(), 0), center + QPointF(handle.width(), 0));
    painter->drawLine(center + QPointF(0, handle.height()), center + QPointF(0, -handle.height()));
}

bool OnMonitorRectItem::getView()
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

