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

#include <QPen>
#include <QBrush>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsView>
#include <QScrollBar>
#include <QGraphicsItem>

Guide::Guide(CustomTrackView *view, const GenTime &pos, const QString &label, double height) :
    QGraphicsLineItem(),
    m_position(pos),
    m_label(label),
    m_view(view),
    m_pen(QPen())
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIgnoresTransformations);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setToolTip(label);
    setLine(0, 0, 0, height);
    if (m_position < GenTime()) {
        m_position = GenTime();
    }
    setPos(m_position.frames(m_view->fps()), 0);
    m_pen.setWidthF(0);
    m_pen.setColor(QColor(0, 0, 200, 180));
    //m_pen.setCosmetic(true);
    setPen(m_pen);
    setZValue(999);
    setAcceptHoverEvents(true);
    const QFontMetrics metric = m_view->fontMetrics();
    m_width = metric.width(QLatin1Char(' ') + m_label + QLatin1Char(' ')) + 2;
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

void Guide::updateGuide(const GenTime &newPos, const QString &comment)
{
    m_position = newPos;
    setPos(m_position.frames(m_view->fps()), 0);
    if (!comment.isEmpty()) {
        m_label = comment;
        setToolTip(m_label);
        const QFontMetrics metric = m_view->fontMetrics();
        m_width = metric.width(QLatin1Char(' ') + m_label + QLatin1Char(' ')) + 2;
        prepareGeometryChange();
    }
}

void Guide::updatePos()
{
    setPos(m_position.frames(m_view->fps()), 0);
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
        if (newPos.x() < 0.0) {
            newPos.setX(0.0);
        }
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

// virtual
QRectF Guide::boundingRect() const
{
    double scale = m_view->matrix().m11();
    double width = m_pen.widthF() / scale * 2;
    QRectF rect(line().x1() - width / 2, line().y1(), width, line().y2() - line().y1());
    if (KdenliveSettings::showmarkers()) {
        // +3 to cover the arc at the end of the comment
        rect.setWidth(width + m_width + 3);
    }
    return rect;
}

// virtual
QPainterPath Guide::shape() const
{
    QPainterPath path;
    if (!scene()) {
        return path;
    }
    double width = m_pen.widthF() * 2;
    path.addRect(line().x1() - width / 2, line().y1(), width, line().y2() - line().y1());
    if (KdenliveSettings::showmarkers() && scene()->views().count()) {
        const QFontMetrics metric = m_view->fontMetrics();
        int offset = scene()->views()[0]->verticalScrollBar()->value();
        QRectF txtBounding(line().x1(), line().y1() + offset, m_width, metric.height());
        path.addRect(txtBounding);
    }
    return path;
}

// virtual
void Guide::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget */*w*/)
{
    QGraphicsLineItem::paint(painter, option);
    if (KdenliveSettings::showmarkers() && scene() && scene()->views().count()) {
        QPointF p1 = line().p1();
        const QFontMetrics metric = m_view->fontMetrics();
        painter->setClipRect(option->rect);
        // makes sure the text stays visible when scrolling vertical
        int offset = scene()->views()[0]->verticalScrollBar()->value();

        QRectF txtBounding = painter->boundingRect(p1.x(), p1.y() + offset, m_width, metric.height(), Qt::AlignLeft | Qt::AlignTop, m_label);
        painter->setBrush(QBrush(m_pen.color()));
        painter->drawRoundedRect(txtBounding.adjusted(-5, -5, 2, 1), 3, 3);
        painter->setPen(Qt::white);
        painter->drawText(txtBounding.adjusted(1, 0, 1, 0), Qt::AlignCenter, m_label);
    }
}

