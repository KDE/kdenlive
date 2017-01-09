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
#include "kdenlivesettings.h"

#include "complex"

#include <QPainter>
#include <QMouseEvent>

BezierSplineEditor::BezierSplineEditor(QWidget *parent) :
    QWidget(parent)
    , m_mode(ModeNormal)
    , m_zoomLevel(0)
    , m_gridLines(3)
    , m_showAllHandles(true)
    , m_pixmapCache(nullptr)
    , m_pixmapIsDirty(true)
    , m_currentPointIndex(-1)
    , m_currentPointType(PTypeP)
    , m_grabOffsetX(0)
    , m_grabOffsetY(0)
{
    setMouseTracking(true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(150, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

BezierSplineEditor::~BezierSplineEditor()
{
    delete m_pixmapCache;
}

CubicBezierSpline BezierSplineEditor::spline() const
{
    return m_spline;
}

void BezierSplineEditor::setSpline(const CubicBezierSpline &spline)
{
    m_spline = spline;
    m_currentPointIndex = -1;
    m_mode = ModeNormal;
    emit currentPoint(BPoint());
    emit modified();
    update();
}

BPoint BezierSplineEditor::getCurrentPoint()
{
    if (m_currentPointIndex >= 0) {
        return m_spline.getPoint(m_currentPointIndex);
    } else {
        return BPoint();
    }
}

void BezierSplineEditor::updateCurrentPoint(const BPoint &p, bool final)
{
    if (m_currentPointIndex >= 0) {
        m_spline.setPoint(m_currentPointIndex, p);
        // during validation the point might have changed
        emit currentPoint(m_spline.getPoint(m_currentPointIndex));
        if (final) {
            emit modified();
        }
        update();
    }
}

void BezierSplineEditor::setPixmap(const QPixmap &pixmap)
{
    m_pixmap = pixmap;
    m_pixmapIsDirty = true;
    update();
}

void BezierSplineEditor::slotZoomIn()
{
    m_zoomLevel = qMax(m_zoomLevel - 1, 0);
    m_pixmapIsDirty = true;
    update();
}

void BezierSplineEditor::slotZoomOut()
{
    m_zoomLevel = qMin(m_zoomLevel + 1, 3);
    m_pixmapIsDirty = true;
    update();
}

void BezierSplineEditor::setShowAllHandles(bool show)
{
    if (m_showAllHandles != show) {
        m_showAllHandles = show;
        update();
    }
}

int BezierSplineEditor::gridLines() const
{
    return m_gridLines;
}

void BezierSplineEditor::setGridLines(int lines)
{
    m_gridLines = qBound(0, lines, 8);
    update();
}

void BezierSplineEditor::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);

    /*
     * Zoom
     */
    int wWidth = width() - 1;
    int wHeight = height() - 1;
    int offsetX = 1 / 8. * m_zoomLevel * wWidth;
    int offsetY = 1 / 8. * m_zoomLevel * wHeight;
    wWidth -= 2 * offsetX;
    wHeight -= 2 * offsetY;

    p.translate(offsetX, offsetY);

    /*
     * Background
     */
    p.fillRect(rect().translated(-offsetX, -offsetY), palette().background());
    if (!m_pixmap.isNull()) {
        if (m_pixmapIsDirty || !m_pixmapCache) {
            delete m_pixmapCache;
            m_pixmapCache = new QPixmap(wWidth + 1, wHeight + 1);
            QPainter cachePainter(m_pixmapCache);

            cachePainter.scale(1.0 * (wWidth + 1) / m_pixmap.width(), 1.0 * (wHeight + 1) / m_pixmap.height());
            cachePainter.drawPixmap(0, 0, m_pixmap);
            m_pixmapIsDirty = false;
        }
        p.drawPixmap(0, 0, *m_pixmapCache);
    }

    p.setPen(QPen(palette().mid().color(), 1, Qt::SolidLine));

    /*
     * Borders
     */
    if (m_zoomLevel != 0) {
        p.drawRect(0, 0, wWidth, wHeight);
    }

    /*
     * Grid
     */
    if (m_gridLines != 0) {
        double stepH = wWidth / (double)(m_gridLines + 1);
        double stepV = wHeight / (double)(m_gridLines + 1);
        for (int i = 1; i <= m_gridLines; ++i) {
            p.drawLine(QLineF(i * stepH, 0, i * stepH, wHeight));
            p.drawLine(QLineF(0, i * stepV, wWidth, i * stepV));
        }
    }

    p.setRenderHint(QPainter::Antialiasing);

    /*
     * Standard line
     */
    p.drawLine(QLineF(0, wHeight, wWidth, 0));

    /*
     * Prepare Spline, Points
     */
    int max = m_spline.points().count() - 1;
    if (max < 1) {
        return;
    }
    BPoint point(m_spline.getPoint(0, wWidth, wHeight, true));

    /*
     * Spline
     */
    BPoint next;

    QPainterPath splinePath(QPointF(point.p.x(), point.p.y()));
    for (int i = 0; i < max; ++i) {
        point = m_spline.getPoint(i, wWidth, wHeight, true);
        next = m_spline.getPoint(i + 1, wWidth, wHeight, true);
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
        point = m_spline.getPoint(i, wWidth, wHeight, true);

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

        p.drawEllipse(QRectF(point.p.x() - 3,
                             point.p.y() - 3, 6, 6));
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

void BezierSplineEditor::resizeEvent(QResizeEvent *event)
{
    m_pixmapIsDirty = true;
    QWidget::resizeEvent(event);
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

    point_types selectedPoint;
    int closestPointIndex = nearestPointInRange(QPointF(x, y), wWidth, wHeight, &selectedPoint);

    if (event->button() == Qt::RightButton && closestPointIndex > 0 && closestPointIndex < m_spline.points().count() - 1 && selectedPoint == PTypeP) {
        m_spline.removePoint(closestPointIndex);
        setCursor(Qt::ArrowCursor);
        m_mode = ModeNormal;
        if (closestPointIndex < m_currentPointIndex) {
            --m_currentPointIndex;
        }
        update();
        if (m_currentPointIndex >= 0) {
            emit currentPoint(m_spline.getPoint(m_currentPointIndex));
        } else {
            emit currentPoint(BPoint());
        }
        emit modified();
        return;
    } else if (event->button() != Qt::LeftButton) {
        return;
    }

    if (closestPointIndex < 0) {
        m_currentPointIndex = m_spline.addPoint(BPoint(QPointF(x - 0.05, y - 0.05),
                                                QPointF(x, y),
                                                QPointF(x + 0.05, y + 0.05)));
        m_currentPointType = PTypeP;
    } else {
        m_currentPointIndex = closestPointIndex;
        m_currentPointType = selectedPoint;
    }

    BPoint point = m_spline.getPoint(m_currentPointIndex);

    m_grabPOriginal = point;
    if (m_currentPointIndex > 0) {
        m_grabPPrevious = m_spline.getPoint(m_currentPointIndex - 1);
    }
    if (m_currentPointIndex < m_spline.points().count() - 1) {
        m_grabPNext = m_spline.getPoint(m_currentPointIndex + 1);
    }
    m_grabOffsetX = point[(int)m_currentPointType].x() - x;
    m_grabOffsetY = point[(int)m_currentPointType].y() - y;

    point[(int)m_currentPointType] = QPointF(x + m_grabOffsetX, y + m_grabOffsetY);

    m_spline.setPoint(m_currentPointIndex, point);

    m_mode = ModeDrag;

    emit currentPoint(point);
    update();
}

void BezierSplineEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    setCursor(Qt::ArrowCursor);
    m_mode = ModeNormal;

    emit modified();
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

    if (m_mode == ModeNormal) {
        // If no point is selected set the cursor shape if on top
        point_types type;
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
        BPoint point = m_spline.getPoint(m_currentPointIndex);
        switch (m_currentPointType) {
        case PTypeH1:
            rightX = point.p.x();
            if (m_currentPointIndex == 0) {
                leftX = -4;
            } else {
                leftX = m_spline.getPoint(m_currentPointIndex - 1).p.x();
            }

            x = qBound(leftX, x, rightX);
            point.setH1(QPointF(x, y));
            break;

        case PTypeP:
            if (m_currentPointIndex == 0) {
                rightX = 0.0;
            } else if (m_currentPointIndex == m_spline.points().count() - 1) {
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

        case PTypeH2:
            leftX = point.p.x();
            if (m_currentPointIndex == m_spline.points().count() - 1) {
                rightX = 5;
            } else {
                rightX = m_spline.getPoint(m_currentPointIndex + 1).p.x();
            }

            x = qBound(leftX, x, rightX);
            point.setH2(QPointF(x, y));
        };

        int index = m_currentPointIndex;
        m_currentPointIndex = m_spline.setPoint(m_currentPointIndex, point);

        if (m_currentPointType == PTypeP) {
            // we might have changed the handles of other points
            // try to restore
            if (index == m_currentPointIndex) {
                if (m_currentPointIndex > 0) {
                    m_spline.setPoint(m_currentPointIndex - 1, m_grabPPrevious);
                }
                if (m_currentPointIndex < m_spline.points().count() - 1) {
                    m_spline.setPoint(m_currentPointIndex + 1, m_grabPNext);
                }
            } else {
                if (m_currentPointIndex < index) {
                    m_spline.setPoint(index, m_grabPPrevious);
                    m_grabPNext = m_grabPPrevious;
                    if (m_currentPointIndex > 0) {
                        m_grabPPrevious = m_spline.getPoint(m_currentPointIndex - 1);
                    }
                } else {
                    m_spline.setPoint(index, m_grabPNext);
                    m_grabPPrevious = m_grabPNext;
                    if (m_currentPointIndex < m_spline.points().count() - 1) {
                        m_grabPNext = m_spline.getPoint(m_currentPointIndex + 1);
                    }
                }
            }
        }

        emit currentPoint(point);
        if (KdenliveSettings::dragvalue_directupdate()) {
            emit modified();
        }
        update();
    }
}

void BezierSplineEditor::mouseDoubleClickEvent(QMouseEvent * /*event*/)
{
    if (m_currentPointIndex >= 0) {
        BPoint p = m_spline.getPoint(m_currentPointIndex);
        p.handlesLinked = !p.handlesLinked;
        m_spline.setPoint(m_currentPointIndex, p);
        emit currentPoint(p);
    }
}

void BezierSplineEditor::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
}

int BezierSplineEditor::nearestPointInRange(const QPointF &p, int wWidth, int wHeight, BezierSplineEditor::point_types *sel)
{
    double nearestDistanceSquared = 1000;
    point_types selectedPoint = PTypeP;
    int nearestIndex = -1;
    int i = 0;

    double distanceSquared;
    // find out distance using the Pythagorean theorem
    foreach (const BPoint &point, m_spline.points()) {
        for (int j = 0; j < 3; ++j) {
            distanceSquared = pow(point[j].x() - p.x(), 2) + pow(point[j].y() - p.y(), 2);
            if (distanceSquared < nearestDistanceSquared) {
                nearestIndex = i;
                nearestDistanceSquared = distanceSquared;
                selectedPoint = (point_types)j;
            }
        }
        ++i;
    }

    if (nearestIndex >= 0 && (nearestIndex == m_currentPointIndex || selectedPoint == PTypeP || m_showAllHandles)) {
        // a point was found and it is not a hidden handle
        BPoint point = m_spline.getPoint(nearestIndex);
        if (qAbs(p.x() - point[(int)selectedPoint].x()) * wWidth < 5 && qAbs(p.y() - point[(int)selectedPoint].y()) * wHeight < 5) {
            *sel = selectedPoint;
            return nearestIndex;
        }
    }

    return -1;
}

