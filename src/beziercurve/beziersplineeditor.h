/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#ifndef BEZIERSPLINEEDITOR_H
#define BEZIERSPLINEEDITOR_H

#include "cubicbezierspline.h"

#include <QtCore>
#include <QWidget>

class BezierSplineEditor : public QWidget
{
    Q_OBJECT

public:
    BezierSplineEditor(QWidget* parent = 0);
    virtual ~BezierSplineEditor();

    CubicBezierSpline spline();
    void setSpline(const CubicBezierSpline &spline);

    BPoint getCurrentPoint();
    void updateCurrentPoint(const BPoint &p);

    int gridLines();
    void setGridLines(int lines);

    void setPixmap(const QPixmap &pixmap);

public slots:
    void slotZoomIn();
    void slotZoomOut();

protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);
    void leaveEvent(QEvent *event);
    void resizeEvent(QResizeEvent *event);

private:
    CubicBezierSpline m_spline;
    enum modes { ModeDrag, ModeNormal };
    enum point_types { PTypeH1, PTypeP, PTypeH2 };
    modes m_mode;
    int m_zoomLevel;
    int m_gridLines;
    QPixmap m_pixmap;
    QPixmap *m_pixmapCache;
    bool m_pixmapIsDirty;

    int m_currentPointIndex;
    point_types m_currentPointType;
    double m_grabOffsetX;
    double m_grabOffsetY;
    BPoint m_grabPOriginal;
    BPoint m_grabPNext;
    BPoint m_grabPPrevious;

    int nearestPointInRange(QPointF p, int wWidth, int wHeight, point_types *sel);

signals:
    void modified();
    void currentPoint(const BPoint &p);
};

#endif
