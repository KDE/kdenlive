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

#include <KDebug>

#include "guide.h"
#include "customtrackview.h"

Guide::Guide(CustomTrackView *view, GenTime pos, QString label, double scale, double fps, double height)
        : QGraphicsLineItem(), m_view(view), m_position(pos), m_label(label), m_scale(scale), m_fps(fps) {
    setFlags(QGraphicsItem::ItemIsMovable);
    setToolTip(label);
    setLine(0, 0, 0, height);
    setPos(m_position.frames(m_fps) * scale, 0);
    setPen(QPen(QBrush(QColor(0, 0, 200, 180)), 2));
    setZValue(999);
    setAcceptsHoverEvents(true);
}


void Guide::updatePosition(double scale) {
    setPos(m_position.frames(m_fps) * scale, 0);
    m_scale = scale;
}

QString Guide::label() const {
    return m_label;
}

GenTime Guide::position() const {
    return m_position;
}

void Guide::update(const GenTime newPos, const QString &comment) {
    m_position = newPos;
    if (comment != QString()) {
        m_label = comment;
        setToolTip(m_label);
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
        const double offset = m_position.frames(m_fps) * m_scale;
        newPos.setX(m_view->getSnapPointForPos(offset + newPos.x()) - offset);
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

