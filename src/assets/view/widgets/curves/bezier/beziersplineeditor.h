/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef BEZIERSPLINEEDITOR_H
#define BEZIERSPLINEEDITOR_H

#include "../abstractcurvewidget.h"
#include "bpoint.h"
#include "utils/colortools.h"
#include "cubicbezierspline.h"

#include <QWidget>

/** @class BezierSplineEditor
    @brief \@todo Describe class BezierSplineEditor
    @todo Describe class BezierSplineEditor
 */
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
