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
#include "customtrackscene.h"
#include "kdenlivesettings.h"

#include <KDebug>

#include <QPen>
#include <QBrush>

Guide::Guide(CustomTrackView *view, GenTime pos, QString label, double fps, double height) :
        QGraphicsLineItem(),
        m_position(pos),
        m_label(label),
        m_fps(fps),
        m_view(view)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemClipsToShape);
    setToolTip(label);
    setLine(0, 0, 0, height);
    setPos(m_position.frames(m_fps), 0);
    setPen(QPen(QBrush(QColor(0, 0, 200, 180)), 2));
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
    setPen(QPen(QBrush(QColor(200, 0, 0, 180)), 2));
}

//virtual
void Guide::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    setPen(QPen(QBrush(QColor(0, 0, 200, 180)), 2));
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
    if (KdenliveSettings::showmarkers()) {
        QRectF rect = QGraphicsLineItem::boundingRect();
        rect.setLeft(line().x1());
        rect.setWidth(m_width / static_cast <CustomTrackScene*>(scene())->scale().x());
        return rect;
    } else return QGraphicsLineItem::boundingRect();
}

// virtual
QPainterPath Guide::shape() const
{
    QPainterPath path;
    path.addRect(line().x1() - pen().widthF() / 2, line().y1(), pen().widthF(), line().y2() - line().y1());
    if (KdenliveSettings::showmarkers()) {
        const QFontMetrics metric = m_view->fontMetrics();
        int height = metric.height();
        path.addRoundedRect(line().x1(), line().y1() + 10, m_width / static_cast <CustomTrackScene*>(scene())->scale().x(), height, 3, 3);
    }
    return path;
}

// virtual
void Guide::paint(QPainter *painter, const QStyleOptionGraphicsItem */*option*/, QWidget */*w*/)
{
    painter->setMatrixEnabled(false);
    QLineF guideline = painter->matrix().map(line());
    painter->setPen(pen());
    painter->drawLine(guideline);
    //painter->fillRect(painter->matrix().mapRect(boundingRect()), QColor(200, 100, 100, 100));
    //QGraphicsLineItem::paint(painter, option, w);
    if (KdenliveSettings::showmarkers()) {
        QPointF p1 = guideline.p1() + QPointF(1, 0);
        QRectF txtBounding = painter->boundingRect(p1.x(), p1.y() + 10, m_width, 50, Qt::AlignLeft | Qt::AlignTop, ' ' + m_label + ' ');
        QPainterPath path;
        path.addRoundedRect(txtBounding, 3, 3);
        painter->fillPath(path, QBrush(pen().color()));
        painter->setPen(Qt::white);
        painter->drawText(txtBounding, Qt::AlignCenter, m_label);
    }
}

