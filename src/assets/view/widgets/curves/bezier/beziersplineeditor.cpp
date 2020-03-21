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
#include "cubicbezierspline.h"
#include "kdenlivesettings.h"

#include "complex"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

BezierSplineEditor::BezierSplineEditor(QWidget *parent)
    : AbstractCurveWidget(parent)

{
    m_curve = CubicBezierSpline();
}

BezierSplineEditor::~BezierSplineEditor() = default;

void BezierSplineEditor::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);

    paintBackground(&p);

    /*
     * Prepare Spline, Points
     */
    int max = m_curve.count() - 1;
    if (max < 1) {
        return;
    }
    BPoint point(m_curve.getPoint(0, m_wWidth, m_wHeight, true));

    /*
     * Spline
     */
    BPoint next;

    QPainterPath splinePath(QPointF(point.p.x(), point.p.y()));
    for (int i = 0; i < max; ++i) {
        point = m_curve.getPoint(i, m_wWidth, m_wHeight, true);
        next = m_curve.getPoint(i + 1, m_wWidth, m_wHeight, true);
        splinePath.cubicTo(point.h2, next.h1, next.p);
    }
    p.setPen(QPen(palette().text().color(), 1, Qt::SolidLine));
    p.drawPath(splinePath);

    /*
     * Points + Handles
     */
    p.setPen(QPen(Qt::red, 1, Qt::SolidLine));

    QPolygonF handle = QPolygonF() << QPointF(0, -3) << QPointF(3, 0) << QPointF(0, 3) << QPointF(-3, 0);

    for (int i = 0; i <= max; ++i) {
        point = m_curve.getPoint(i, m_wWidth, m_wHeight, true);

        if (i == m_currentPointIndex) {
            // selected point: fill p and handles
            p.setBrush(QBrush(QColor(Qt::red), Qt::SolidPattern));
            // connect p and handles with lines
            if (i != 0) {
                p.drawLine(QLineF(point.h1.x(), point.h1.y(), point.p.x(), point.p.y()));
            }
            if (i != max) {
                p.drawLine(QLineF(point.p.x(), point.p.y(), point.h2.x(), point.h2.y()));
            }
        }

        p.drawEllipse(QRectF(point.p.x() - 3, point.p.y() - 3, 6, 6));
        if (i != 0 && (i == m_currentPointIndex || m_showAllHandles)) {
            p.drawConvexPolygon(handle.translated(point.h1.x(), point.h1.y()));
        }
        if (i != max && (i == m_currentPointIndex || m_showAllHandles)) {
            p.drawConvexPolygon(handle.translated(point.h2.x(), point.h2.y()));
        }

        if (i == m_currentPointIndex) {
            p.setBrush(QBrush(Qt::NoBrush));
        }
    }
}

void BezierSplineEditor::mousePressEvent(QMouseEvent *event)
{
    int wWidth = width() - 1;
    int wHeight = height() - 1;
    int offsetX = 1 / 8. * m_zoomLevel * wWidth;
    int offsetY = 1 / 8. * m_zoomLevel * wHeight;
    wWidth -= 2 * offsetX;
    wHeight -= 2 * offsetY;

    double x = (event->pos().x() - offsetX) / (double)(wWidth);
    double y = 1.0 - (event->pos().y() - offsetY) / (double)(wHeight);

    BPoint::PointType selectedPoint;
    int closestPointIndex = nearestPointInRange(QPointF(x, y), wWidth, wHeight, &selectedPoint);

    if (event->button() == Qt::RightButton && closestPointIndex > 0 && closestPointIndex < m_curve.count() - 1 && selectedPoint == BPoint::PointType::P) {
        m_currentPointIndex = closestPointIndex;
        slotDeleteCurrentPoint();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (closestPointIndex < 0) {
        if (m_curve.count() < m_maxPoints) {
            m_currentPointIndex = m_curve.addPoint(QPointF(x, y));
            m_currentPointType = BPoint::PointType::P;
        }
    } else {
        m_currentPointIndex = closestPointIndex;
        m_currentPointType = selectedPoint;
    }

    BPoint point = m_curve.getPoint(m_currentPointIndex);

    m_grabPOriginal = point;
    if (m_currentPointIndex > 0) {
        m_grabPPrevious = m_curve.getPoint(m_currentPointIndex - 1);
    }
    if (m_currentPointIndex < m_curve.count() - 1) {
        m_grabPNext = m_curve.getPoint(m_currentPointIndex + 1);
    }
    m_grabOffsetX = point[(int)m_currentPointType].x() - x;
    m_grabOffsetY = point[(int)m_currentPointType].y() - y;

    point[(int)m_currentPointType] = QPointF(x + m_grabOffsetX, y + m_grabOffsetY);

    m_curve.setPoint(m_currentPointIndex, point);

    m_state = State_t::DRAG;

    emit currentPoint(point, isCurrentPointExtremal());
    emit modified();
    update();
}

void BezierSplineEditor::mouseMoveEvent(QMouseEvent *event)
{
    int wWidth = width() - 1;
    int wHeight = height() - 1;
    int offsetX = 1 / 8. * m_zoomLevel * wWidth;
    int offsetY = 1 / 8. * m_zoomLevel * wHeight;
    wWidth -= 2 * offsetX;
    wHeight -= 2 * offsetY;

    double x = (event->pos().x() - offsetX) / (double)(wWidth);
    double y = 1.0 - (event->pos().y() - offsetY) / (double)(wHeight);

    if (m_state == State_t::NORMAL) {
        // If no point is selected set the cursor shape if on top
        BPoint::PointType type;
        int nearestPointIndex = nearestPointInRange(QPointF(x, y), wWidth, wHeight, &type);

        if (nearestPointIndex < 0) {
            setCursor(Qt::ArrowCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
    } else {
        // Else, drag the selected point
        setCursor(Qt::CrossCursor);

        x += m_grabOffsetX;
        y += m_grabOffsetY;

        double leftX = 0.;
        double rightX = 1.;
        BPoint point = m_curve.getPoint(m_currentPointIndex);
        switch (m_currentPointType) {
        case BPoint::PointType::H1:
            rightX = point.p.x();
            if (m_currentPointIndex == 0) {
                leftX = -4;
            } else {
                leftX = m_curve.getPoint(m_currentPointIndex - 1).p.x();
            }

            x = qBound(leftX, x, rightX);
            point.setH1(QPointF(x, y));
            break;

        case BPoint::PointType::P:
            if (m_currentPointIndex == 0) {
                rightX = 0.0;
            } else if (m_currentPointIndex == m_curve.count() - 1) {
                leftX = 1.0;
            }

            x = qBound(leftX, x, rightX);
            y = qBound(0., y, 1.);

            // handles might have changed because we neared another point
            // try to restore
            point.h1 = m_grabPOriginal.h1;
            point.h2 = m_grabPOriginal.h2;
            // and move by same offset
            // (using update handle in point.setP won't work because the offset between new and old point is very small)
            point.h1 += QPointF(x, y) - m_grabPOriginal.p;
            point.h2 += QPointF(x, y) - m_grabPOriginal.p;

            point.setP(QPointF(x, y), false);
            break;

        case BPoint::PointType::H2:
            leftX = point.p.x();
            if (m_currentPointIndex == m_curve.count() - 1) {
                rightX = 5;
            } else {
                rightX = m_curve.getPoint(m_currentPointIndex + 1).p.x();
            }

            x = qBound(leftX, x, rightX);
            point.setH2(QPointF(x, y));
        };

        int index = m_currentPointIndex;
        m_currentPointIndex = m_curve.setPoint(m_currentPointIndex, point);

        if (m_currentPointType == BPoint::PointType::P) {
            // we might have changed the handles of other points
            // try to restore
            if (index == m_currentPointIndex) {
                if (m_currentPointIndex > 0) {
                    m_curve.setPoint(m_currentPointIndex - 1, m_grabPPrevious);
                }
                if (m_currentPointIndex < m_curve.count() - 1) {
                    m_curve.setPoint(m_currentPointIndex + 1, m_grabPNext);
                }
            } else {
                if (m_currentPointIndex < index) {
                    m_curve.setPoint(index, m_grabPPrevious);
                    m_grabPNext = m_grabPPrevious;
                    if (m_currentPointIndex > 0) {
                        m_grabPPrevious = m_curve.getPoint(m_currentPointIndex - 1);
                    }
                } else {
                    m_curve.setPoint(index, m_grabPNext);
                    m_grabPPrevious = m_grabPNext;
                    if (m_currentPointIndex < m_curve.count() - 1) {
                        m_grabPNext = m_curve.getPoint(m_currentPointIndex + 1);
                    }
                }
            }
        }

        emit currentPoint(point, isCurrentPointExtremal());
        if (KdenliveSettings::dragvalue_directupdate()) {
            emit modified();
        }
        update();
    }
}

void BezierSplineEditor::mouseDoubleClickEvent(QMouseEvent * /*event*/)
{
    if (m_currentPointIndex >= 0) {
        BPoint p = m_curve.getPoint(m_currentPointIndex);
        p.handlesLinked = !p.handlesLinked;
        m_curve.setPoint(m_currentPointIndex, p);
        emit currentPoint(p, isCurrentPointExtremal());
    }
}

int BezierSplineEditor::nearestPointInRange(const QPointF &p, int wWidth, int wHeight, BPoint::PointType *sel)
{

    auto nearest = m_curve.closestPoint(p);
    int nearestIndex = nearest.first;
    BPoint::PointType pointType = nearest.second;

    if (nearestIndex >= 0 && (nearestIndex == m_currentPointIndex || pointType == BPoint::PointType::P || m_showAllHandles)) {
        // a point was found and it is not a hidden handle
        BPoint point = m_curve.getPoint(nearestIndex);
        double dx = (p.x() - point[(int)pointType].x()) * wWidth;
        double dy = (p.y() - point[(int)pointType].y()) * wHeight;
        if (dx * dx + dy * dy <= m_grabRadius * m_grabRadius) {
            *sel = pointType;
            return nearestIndex;
        }
    }

    return -1;
}

void BezierSplineEditor::setShowAllHandles(bool show)
{
    if (m_showAllHandles != show) {
        m_showAllHandles = show;
        update();
    }
}

QList<BPoint> BezierSplineEditor::getPoints() const
{
    return m_curve.getPoints();
}
