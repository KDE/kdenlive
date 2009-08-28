/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "guide.h"
#include "customtrackview.h"
#include "kdenlivesettings.h"

#include <KDebug>

#include <QPen>
#include <QBrush>
#include <QStyleOptionGraphicsItem>

Guide::Guide(CustomTrackView *view, GenTime pos, QString label, double fps, double height) :
        QGraphicsLineItem(),
        m_position(pos),
        m_label(label),
        m_fps(fps),
        m_view(view),
        m_pen(QPen())
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIgnoresTransformations);
    setToolTip(label);
    setLine(0, 0, 0, height);
    setPos(m_position.frames(m_fps), 0);
    m_pen.setWidthF(0);
    m_pen.setColor(QColor(0, 0, 200, 180));
    //m_pen.setCosmetic(true);
    setPen(m_pen);
    setZValue(999);
    setAcceptsHoverEvents(true);
    const QFontMetrics metric = m_view->fontMetrics();
    m_width = metric.width(' ' + m_label + ' ') + 2;
    prepareGeometryChange();
}

QString Guide::label() const
{
    return m_label;
}

GenTime Guide::position() const
{
    return m_position;
}

CommentedTime Guide::info() const
{
    return CommentedTime(m_position, m_label);
}

void Guide::updateGuide(const GenTime newPos, const QString &comment)
{
    m_position = newPos;
    setPos(m_position.frames(m_fps), 0);
    if (!comment.isEmpty()) {
        m_label = comment;
        setToolTip(m_label);
        const QFontMetrics metric = m_view->fontMetrics();
        m_width = metric.width(' ' + m_label + ' ') + 2;
        prepareGeometryChange();
    }
}

//virtual
int Guide::type() const
{
    return GUIDEITEM;
}

//virtual
void Guide::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
    m_pen.setColor(QColor(200, 0, 0, 180));
    setPen(m_pen);
}

//virtual
void Guide::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    m_pen.setColor(QColor(0, 0, 200, 180));
    setPen(m_pen);
}

//virtual
QVariant Guide::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {
        // value is the new position.
        QPointF newPos = value.toPointF();
        newPos.setY(0);
        newPos.setX(m_view->getSnapPointForPos(newPos.x()));
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

// virtual
QRectF Guide::boundingRect() const
{
    double scale = m_view->matrix().m11();
    double width = m_pen.widthF() / scale * 2;
    QRectF rect(line().x1() - width / 2 , line().y1(), width, line().y2() - line().y1());
    if (KdenliveSettings::showmarkers()) {
        rect.setWidth(width + m_width / scale);
    }
    return rect;
}

// virtual
QPainterPath Guide::shape() const
{
    QPainterPath path;
    if (!scene()) return path;
    double scale = m_view->matrix().m11();
    double width = m_pen.widthF() / scale * 2;
    path.addRect(line().x1() - width / 2 , line().y1(), width, line().y2() - line().y1());
    if (KdenliveSettings::showmarkers()) {
        const QFontMetrics metric = m_view->fontMetrics();
        QRectF txtBounding(line().x1() + 1 / scale, line().y1() + 10, m_width / scale, metric.height() / m_view->matrix().m22());
        path.addRect(txtBounding);
    }
    return path;
}

// virtual
void Guide::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget */*w*/)
{
    QGraphicsLineItem::paint(painter, option);
    if (KdenliveSettings::showmarkers()) {
        QPointF p1 = line().p1() + QPointF(1, 0);
        const QFontMetrics metric = m_view->fontMetrics();
        QRectF txtBounding = painter->boundingRect(p1.x(), p1.y() + 10, m_width, metric.height(), Qt::AlignLeft | Qt::AlignTop, ' ' + m_label + ' ');
        QPainterPath path;
        path.addRoundedRect(txtBounding, 3, 3);
        painter->fillPath(path, m_pen.color());
        painter->setPen(Qt::white);
        painter->drawText(txtBounding, Qt::AlignCenter, m_label);
    }
}

