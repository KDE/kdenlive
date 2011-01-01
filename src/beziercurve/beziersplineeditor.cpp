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

#include "beziersplineeditor.h"

#include <QPainter>
#include <QMouseEvent>

#include <KDebug>

BezierSplineEditor::BezierSplineEditor(QWidget* parent) :
        QWidget(parent),
        m_mode(ModeNormal),
        m_currentPointIndex(-1)
{
    setMouseTracking(true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(150, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

CubicBezierSpline BezierSplineEditor::spline()
{
    return m_spline;
}

void BezierSplineEditor::setSpline(const CubicBezierSpline& spline)
{
    // TODO: cleanup
    m_spline.fromString(spline.toString());
}

BPoint BezierSplineEditor::getCurrentPoint()
{
    if (m_currentPointIndex >= 0)
        return m_spline.points()[m_currentPointIndex];
    else
        return BPoint();
}

void BezierSplineEditor::updateCurrentPoint(const BPoint& p)
{
    if (m_currentPointIndex >= 0) {
        m_spline.setPoint(m_currentPointIndex, p);
        // during validation the point might have changed
        emit currentPoint(m_spline.points()[m_currentPointIndex]);
        update();
    }
}

void BezierSplineEditor::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);

    p.fillRect(rect(), palette().background());

    int    wWidth = width() - 1;
    int    wHeight = height() - 1;

    /*
     * Spline
     */
    double prevY = wHeight - m_spline.value(0.) * wHeight;
    double prevX = 0.;
    double curY;
    double normalizedX = -1;
    int x;
    
    p.setPen(QPen(Qt::black, 1, Qt::SolidLine));
    for (x = 0 ; x < wWidth ; ++x) {
        normalizedX = x / (double)wWidth;
        curY = wHeight - m_spline.value(normalizedX, true) * wHeight;
        
        /*
         * Keep in mind that QLineF rounds doubles
         * to ints mathematically, not just rounds down
         * like in C
         */
        p.drawLine(QLineF(prevX, prevY,
                          x, curY));
        prevX = x;
        prevY = curY;
    }
    p.drawLine(QLineF(prevX, prevY ,
                      x, wHeight - m_spline.value(1.0, true) * wHeight));

    /*
     * Points + Handles
     */
    p.setPen(QPen(Qt::red, 1, Qt::SolidLine));
    BPoint point;
    QPolygon handle(4);
    handle.setPoints(4,
                     1,  -2,
                     4,  1,
                     1,  4,
                     -2, 1);
    for (int i = 0; i < m_spline.points().count(); ++i) {
        point = m_spline.points().at(i);
        if (i == m_currentPointIndex)
            p.setBrush(QBrush(QColor(Qt::red), Qt::SolidPattern));

        p.drawConvexPolygon(handle.translated(point.h1.x() * wWidth, wHeight - point.h1.y() * wHeight));
        p.drawEllipse(QRectF(point.p.x() * wWidth - 3,
                            wHeight - 3 - point.p.y() * wHeight, 6, 6));
        p.drawConvexPolygon(handle.translated(point.h2.x() * wWidth, wHeight - point.h2.y() * wHeight));

        if ( i == m_currentPointIndex)
            p.setBrush(QBrush(Qt::NoBrush));
    }
}

void BezierSplineEditor::resizeEvent(QResizeEvent* event)
{
    m_spline.setPrecision(width());
    QWidget::resizeEvent(event);
}

void BezierSplineEditor::mousePressEvent(QMouseEvent* event)
{
    double x = event->pos().x() / (double)(width() - 1);
    double y = 1.0 - event->pos().y() / (double)(height() - 1);

    point_types selectedPoint;
    int closestPointIndex = nearestPointInRange(QPointF(x, y), width(), height(), &selectedPoint);

    if (event->button() == Qt::RightButton && closestPointIndex > 0 && closestPointIndex < m_spline.points().count() - 1 && selectedPoint == PTypeP) {
        m_spline.removePoint(closestPointIndex);
        setCursor(Qt::ArrowCursor);
        m_mode = ModeNormal;
        if (closestPointIndex < m_currentPointIndex)
            --m_currentPointIndex;
        update();
        if (m_currentPointIndex >= 0)
            emit currentPoint(m_spline.points()[m_currentPointIndex]);
        else
            emit currentPoint(BPoint());
        emit modified();
        return;
    } else if (event->button() != Qt::LeftButton) {
        return;
    }

    if (closestPointIndex < 0) {
        BPoint po;
        po.p = QPointF(x, y);
        po.h1 = QPointF(x-0.05, y-0.05);
        po.h2 = QPointF(x+0.05, y+0.05);
        m_currentPointIndex = m_spline.addPoint(po);
        m_currentPointType = PTypeP;
        /*if (!d->jumpOverExistingPoints(newPoint, -1)) return;*/
    } else {
        m_currentPointIndex = closestPointIndex;
        m_currentPointType = selectedPoint;
    }

    BPoint point = m_spline.points()[m_currentPointIndex];
    QPointF p;
    switch (m_currentPointType) {
    case PTypeH1:
        p = point.h1;
        break;
    case PTypeP:
        p = point.p;
        break;
    case PTypeH2:
        p = point.h2;
    }

    m_grabOriginalX = p.x();
    m_grabOriginalY = p.y();
    m_grabOffsetX = p.x() - x;
    m_grabOffsetY = p.y() - y;

    switch (m_currentPointType) {
        case PTypeH1:
            point.h1 = QPointF(x + m_grabOffsetX, y + m_grabOffsetY);
            break;
        case PTypeP:
            point.p = QPointF(x + m_grabOffsetX, y + m_grabOffsetY);
            break;
        case PTypeH2:
            point.h2 = QPointF(x + m_grabOffsetX, y + m_grabOffsetY);
    }
    m_spline.setPoint(m_currentPointIndex, point);

    //d->m_draggedAwayPointIndex = -1;

    m_mode = ModeDrag;

    emit currentPoint(point);
    update();
}

void BezierSplineEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    setCursor(Qt::ArrowCursor);
    m_mode = ModeNormal;

    emit modified();
}

void BezierSplineEditor::mouseMoveEvent(QMouseEvent* event)
{
    double x = event->pos().x() / (double)(width() - 1);
    double y = 1.0 - event->pos().y() / (double)(height() - 1);
    
    if (m_mode == ModeNormal) { // If no point is selected set the the cursor shape if on top
        point_types type;
        int nearestPointIndex = nearestPointInRange(QPointF(x, y), width(), height(), &type);
        
        if (nearestPointIndex < 0)
            setCursor(Qt::ArrowCursor);
        else
            setCursor(Qt::CrossCursor);
    } else { // Else, drag the selected point
        /*bool crossedHoriz = event->pos().x() - width() > MOUSE_AWAY_THRES ||
        event->pos().x() < -MOUSE_AWAY_THRES;
        bool crossedVert =  event->pos().y() - height() > MOUSE_AWAY_THRES ||
        event->pos().y() < -MOUSE_AWAY_THRES;
        
        bool removePoint = (crossedHoriz || crossedVert);
        
        if (!removePoint && d->m_draggedAwayPointIndex >= 0) {
            // point is no longer dragged away so reinsert it
            QPointF newPoint(d->m_draggedAwayPoint);
            d->m_grab_point_index = d->m_curve.addPoint(newPoint);
            d->m_draggedAwayPointIndex = -1;
        }
        
        if (removePoint &&
            (d->m_draggedAwayPointIndex >= 0))
            return;
        */
        
        setCursor(Qt::CrossCursor);
        
        x += m_grabOffsetX;
        y += m_grabOffsetY;
        
        double leftX, rightX;
        BPoint point = m_spline.points()[m_currentPointIndex];
        switch (m_currentPointType) {
        case PTypeH1:
            rightX = point.p.x();
            if (m_currentPointIndex == 0)
                leftX = -1000;
            else
                leftX = m_spline.points()[m_currentPointIndex - 1].p.x();
            x = qBound(leftX, x, rightX);
            point.h1 = QPointF(x, y);
            break;
        case PTypeP:
            if (m_currentPointIndex == 0) {
                leftX = 0.0;
                rightX = 0.0;
                /*if (d->m_curve.points().count() > 1)
                 *           rightX = d->m_curve.points()[d->m_grab_point_index + 1].x() - POINT_AREA;
                 *       else
                 *           rightX = 1.0;*/
            } else if (m_currentPointIndex == m_spline.points().count() - 1) {
                leftX = 1.0;//m_spline.points()[m_currentPointIndex - 1].p.x();
                rightX = 1.0;
            } else {
                //// the 1E-4 addition so we can grab the dot later.
                leftX = m_spline.points()[m_currentPointIndex - 1].p.x();// + POINT_AREA;
                rightX = m_spline.points()[m_currentPointIndex + 1].p.x();// - POINT_AREA;
            }
            x = qBound(leftX, x, rightX);
            y = qBound(0., y, 1.);

            // move handles by same offset
            point.h1 += QPointF(x, y) - point.p;
            point.h2 += QPointF(x, y) - point.p;

            point.p = QPointF(x, y);
            break;
        case PTypeH2:
            leftX = point.p.x();
            if (m_currentPointIndex == m_spline.points().count() - 1)
                rightX = 1001;
            else
                rightX = m_spline.points()[m_currentPointIndex + 1].p.x();
            x = qBound(leftX, x, rightX);
            point.h2 = QPointF(x, y);
        };

        m_spline.setPoint(m_currentPointIndex, point);
        
        /*if (removePoint && d->m_curve.points().count() > 2) {
            d->m_draggedAwayPoint = d->m_curve.points()[d->m_grab_point_index];
            d->m_draggedAwayPointIndex = d->m_grab_point_index;
            d->m_curve.removePoint(d->m_grab_point_index);
            d->m_grab_point_index = bounds(d->m_grab_point_index, 0, d->m_curve.points().count() - 1);
        }
        
        d->setCurveModified();*/
        emit currentPoint(point);
        update();
    }
}

void BezierSplineEditor::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
}

int BezierSplineEditor::nearestPointInRange(QPointF p, int wWidth, int wHeight, BezierSplineEditor::point_types* sel)
{
    double nearestDistanceSquared = 1000;
    point_types selectedPoint;
    int nearestIndex = -1;
    int i = 0;

    double distanceSquared;
    foreach(const BPoint & point, m_spline.points()) {
        distanceSquared = pow(point.h1.x() - p.x(), 2) + pow(point.h1.y() - p.y(), 2);
        if (distanceSquared < nearestDistanceSquared) {
            nearestIndex = i;
            nearestDistanceSquared = distanceSquared;
            selectedPoint = PTypeH1;
        }
        distanceSquared = pow(point.p.x() - p.x(), 2) + pow(point.p.y() - p.y(), 2);
        if (distanceSquared < nearestDistanceSquared) {
            nearestIndex = i;
            nearestDistanceSquared = distanceSquared;
            selectedPoint = PTypeP;
        }
        distanceSquared = pow(point.h2.x() - p.x(), 2) + pow(point.h2.y() - p.y(), 2);
        if (distanceSquared < nearestDistanceSquared) {
            nearestIndex = i;
            nearestDistanceSquared = distanceSquared;
            selectedPoint = PTypeH2;
        }
        ++i;
    }

    if (nearestIndex >= 0) {
        BPoint point = m_spline.points()[nearestIndex];
        QPointF p2;
        switch (selectedPoint) {
        case PTypeH1:
            p2 = point.h1;
            break;
        case PTypeP:
            p2 = point.p;
            break;
        case PTypeH2:
            p2 = point.h2;
        }
        if (qAbs(p.x() - p2.x()) * (wWidth - 1) < 5 && qAbs(p.y() - p2.y()) * (wHeight - 1) < 5) {
            *sel = selectedPoint;
            return nearestIndex;
        }
    }

    return -1;
}

#include "beziersplineeditor.moc"
