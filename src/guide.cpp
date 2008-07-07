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


#include <QPen>
#include <QBrush>
#include <QScrollBar>

#include <KDebug>

#include "guide.h"
#include "customtrackview.h"
#include "kdenlivesettings.h"

Guide::Guide(CustomTrackView *view, GenTime pos, QString label, double scale, double fps, double height)
        : QGraphicsLineItem(), m_view(view), m_position(pos), m_label(label), m_scale(scale), m_fps(fps) {
    setFlags(QGraphicsItem::ItemIsMovable);
    setToolTip(label);
    setLine(0, 0, 0, height);
    setPos(m_position.frames(m_fps) * scale, 0);
    setPen(QPen(QBrush(QColor(0, 0, 200, 180)), 2));
    setZValue(999);
    setAcceptsHoverEvents(true);
    const QFontMetrics metric = m_view->fontMetrics();
    m_width = metric.width(" " + m_label + " ") + 2;
    prepareGeometryChange();
}


void Guide::updatePosition(double scale) {
    m_scale = scale;
    setPos(m_position.frames(m_fps) * m_scale, 0);
}

QString Guide::label() const {
    return m_label;
}

GenTime Guide::position() const {
    return m_position;
}

CommentedTime Guide::info() const {
    return CommentedTime(m_position, m_label);
}

void Guide::updateGuide(const GenTime newPos, const QString &comment) {
    m_position = newPos;
    setPos(m_position.frames(m_fps) * m_scale, 0);
    if (comment != QString()) {
        m_label = comment;
        setToolTip(m_label);
        const QFontMetrics metric = m_view->fontMetrics();
        m_width = metric.width(" " + m_label + " ") + 2;
        prepareGeometryChange();
    }
}

//virtual
int Guide::type() const {
    return GUIDEITEM;
}

//virtual
void Guide::hoverEnterEvent(QGraphicsSceneHoverEvent *) {
    setPen(QPen(QBrush(QColor(200, 0, 0, 180)), 2));
}

//virtual
void Guide::hoverLeaveEvent(QGraphicsSceneHoverEvent *) {
    setPen(QPen(QBrush(QColor(0, 0, 200, 180)), 2));
}

//virtual
QVariant Guide::itemChange(GraphicsItemChange change, const QVariant &value) {
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
QRectF Guide::boundingRect() const {
    if (KdenliveSettings::showmarkers()) {
        QRectF rect = QGraphicsLineItem::boundingRect();
        rect.setWidth(m_width);
        return rect;
    } else return QGraphicsLineItem::boundingRect();
}

// virtual
void Guide::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *w) {
    QGraphicsLineItem::paint(painter, option, w);
    if (KdenliveSettings::showmarkers()) {
        QRectF br = boundingRect();
        QRectF txtBounding = painter->boundingRect(br.x(), br.y() + 10 + m_view->verticalScrollBar()->value(), m_width, 50, Qt::AlignLeft | Qt::AlignTop, " " + m_label + " ");
        QPainterPath path;
        path.addRoundedRect(txtBounding, 3, 3);
        painter->fillPath(path, QBrush(pen().color()));
        painter->setPen(Qt::white);
        painter->drawText(txtBounding, Qt::AlignCenter, m_label);
    }
}

