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

#ifndef GUIDE_H
#define GUIDE_H

#include <QGraphicsLineItem>
#include <QPen>

#include "gentime.h"
#include "definitions.h"

#define GUIDEITEM 8000

class CustomTrackView;

class Guide : public QGraphicsLineItem
{

public:
    Guide(CustomTrackView *view, const GenTime &pos, const QString &label, double height);
    GenTime position() const;
    void updateGuide(const GenTime &newPos, const QString &comment = QString());
    QString label() const;
    CommentedTime info() const;
    void updatePos();
    int type() const Q_DECL_OVERRIDE;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *w) Q_DECL_OVERRIDE;
    QRectF boundingRect() const Q_DECL_OVERRIDE;
    QPainterPath shape() const Q_DECL_OVERRIDE;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *) Q_DECL_OVERRIDE;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) Q_DECL_OVERRIDE;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) Q_DECL_OVERRIDE;

private:
    GenTime m_position;
    QString m_label;
    CustomTrackView *m_view;
    int m_width;
    QPen m_pen;
};

#endif
