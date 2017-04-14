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
    int type() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *w) override;
    QRectF boundingRect() const override;
    QPainterPath shape() const override;

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    GenTime m_position;
    QString m_label;
    CustomTrackView *m_view;
    int m_width;
    QPen m_pen;
};

#endif
