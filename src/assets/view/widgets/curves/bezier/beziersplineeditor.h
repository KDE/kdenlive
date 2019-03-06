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

#include "../abstractcurvewidget.h"
#include "bpoint.h"
#include "colortools.h"
#include "cubicbezierspline.h"

#include <QWidget>

class BezierSplineEditor : public AbstractCurveWidget<CubicBezierSpline>
{
    Q_OBJECT

public:
    using Point_t = BPoint;
    explicit BezierSplineEditor(QWidget *parent = nullptr);
    ~BezierSplineEditor() override;

    /** @brief Sets the property showAllHandles to @param show.
     *
     * showAllHandles: Whether to show only handles for the selected point for all points.
     */
    void setShowAllHandles(bool show);
    QList<BPoint> getPoints() const override;

public slots:

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    /** Whether to show handles for all points or only for the selected one. */
    bool m_showAllHandles{true};

    BPoint::PointType m_currentPointType{BPoint::PointType::P};
    double m_grabOffsetX{0};
    double m_grabOffsetY{0};
    /** selected point before it was modified by dragging (at the time of the mouse press) */
    BPoint m_grabPOriginal;
    /** point with the index currentPointIndex + 1 at the time of the mouse press */
    BPoint m_grabPNext;
    /** point with the index currentPointIndex - 1 at the time of the mouse press */
    BPoint m_grabPPrevious;

    /** @brief Finds the point nearest to @param p and returns it's index.
     * @param sel Is filled with the type of the closest point (h1, p, h2)
     *
     * If no point is near enough -1 is returned. */
    int nearestPointInRange(const QPointF &p, int wWidth, int wHeight, BPoint::PointType *sel);
};

#endif
