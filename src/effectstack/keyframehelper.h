/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef KEYFRAMEHELPER_H
#define KEYFRAMEHELPER_H

#include <QWidget>

#include <mlt++/Mlt.h>

#include "timecode.h"

class KeyframeHelper : public QWidget
{
    Q_OBJECT
public:
    explicit KeyframeHelper(QWidget *parent = nullptr);
    int value() const;
    int frameLength;

protected:
    void paintEvent(QPaintEvent * /*e*/) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *e) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;

private:
    Mlt::Geometry *m_geom;
    int m_position;
    int m_size;
    double m_scale;
    bool m_movingKeyframe;
    Mlt::GeometryItem m_movingItem;
    QList<Mlt::GeometryItem *> m_extraMovingItems;
    QPoint m_dragStart;
    int m_lineHeight;
    int m_hoverKeyframe;
    QColor m_selected;
    QColor m_keyframe;
    QList<Mlt::Geometry *>m_extraGeometries;
    int m_seekPosition;
    int m_offset;

public slots:
    void setKeyGeometry(Mlt::Geometry *geom, int in, int out, bool useOffset = false);
    void addGeometry(Mlt::Geometry *geom);
    void setValue(const int pos);

signals:
    void requestSeek(int);
    void keyframeMoved(int);
    void addKeyframe(int);
    void removeKeyframe(int);
};

#endif
