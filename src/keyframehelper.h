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
    KeyframeHelper(QWidget *parent = 0);
    int value() const;
    int frameLength;

protected:
    virtual void paintEvent(QPaintEvent * /*e*/);
    virtual void wheelEvent(QWheelEvent * e);
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseMoveEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent * event);
    virtual void mouseDoubleClickEvent(QMouseEvent * event);
    virtual void leaveEvent( QEvent * event );

private:
    Mlt::Geometry *m_geom;
    int m_position;
    double m_scale;
    bool m_movingKeyframe;
    Mlt::GeometryItem m_movingItem;
    QList <Mlt::GeometryItem *> m_extraMovingItems;
    QPoint m_dragStart;
    int m_lineHeight;
    bool m_drag;
    int m_hoverKeyframe;
    QColor m_selected;
    QColor m_keyframe;
    QColor m_keyframebg;
    QList <Mlt::Geometry *>m_extraGeometries;
    int m_seekPosition;
        
public slots:
    void setKeyGeometry(Mlt::Geometry *geom, const int length);
    void addGeometry(Mlt::Geometry *geom);
    void setValue(const int pos);

signals:
    void requestSeek(int);
    void keyframeMoved(int);
    void addKeyframe(int);
    void removeKeyframe(int);
};

#endif
