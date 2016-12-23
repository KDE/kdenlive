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

#include <QWidget>

class BezierSplineEditor : public QWidget
{
    Q_OBJECT

public:
    explicit BezierSplineEditor(QWidget *parent = Q_NULLPTR);
    ~BezierSplineEditor();

    CubicBezierSpline spline() const;
    void setSpline(const CubicBezierSpline &spline);

    /** @brief Returns the selected point or else BPoint. */
    BPoint getCurrentPoint();

    /** @brief Replaces current point with @param p (index stays the same).
     * @param final (default = true) emit signal modified? */
    void updateCurrentPoint(const BPoint &p, bool final = true);

    /** @brief Number of lines used in grid. */
    int gridLines() const;

    /** @brief Sets the number of grid lines to draw (in one direction) to @param lines. */
    void setGridLines(int lines);

    /** @brief Sets the background pixmap to @param pixmap. */
    void setPixmap(const QPixmap &pixmap);

    /** @brief Sets the property showAllHandles to @param show.
     *
     * showAllHandles: Whether to show only handles for the selected point for all points. */
    void setShowAllHandles(bool show);


public slots:
    /** @brief Delete current spline point if it is not a extremal point (first or last)
     */
    void slotDeleteCurrentPoint();
    void slotZoomIn();
    void slotZoomOut();

protected:
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;

    /**
       Utility function to check if current selected point is the first or the last
     */
    bool isCurrentPointExtremal();
private:
    CubicBezierSpline m_spline;
    enum modes { ModeDrag, ModeNormal };
    enum point_types { PTypeH1, PTypeP, PTypeH2 };
    modes m_mode;
    int m_zoomLevel;
    int m_gridLines;
    /** Whether to show handles for all points or only for the selected one. */
    bool m_showAllHandles;
    /** Background */
    QPixmap m_pixmap;
    /** A copy of m_pixmap but scaled to fit the size of the edit region */
    QPixmap *m_pixmapCache;
    /** Whether we have to regenerate the pixmap cache because the pixmap or the size of the edit region changed. */
    bool m_pixmapIsDirty;

    int m_currentPointIndex;
    point_types m_currentPointType;
    double m_grabOffsetX;
    double m_grabOffsetY;
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
    int nearestPointInRange(const QPointF &p, int wWidth, int wHeight, point_types *sel);

signals:
    void modified();
    /**
       Signal sent when the current point changes. The point is returned, as well as a flag that determines if the point is the first or last.
     */
    void currentPoint(const BPoint &p, bool extremal);
};

#endif
