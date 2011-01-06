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


BezierSplineEditor::BezierSplineEditor(QWidget* parent) :
        QWidget(parent),
        m_mode(ModeNormal),
        m_zoomLevel(0),
        m_gridLines(3),
        m_pixmapCache(NULL),
        m_pixmapIsDirty(true),
        m_currentPointIndex(-1)
{
    setMouseTracking(true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(150, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

BezierSplineEditor::~BezierSplineEditor()
{
    if (m_pixmapCache)
        delete m_pixmapCache;
}

CubicBezierSpline BezierSplineEditor::spline()
{
    return m_spline;
}

void BezierSplineEditor::setSpline(const CubicBezierSpline& spline)
{
    int precision = m_spline.getPrecision();
    m_spline = spline;
    m_spline.setPrecision(precision);
    m_currentPointIndex = -1;
    m_mode = ModeNormal;
    emit currentPoint(BPoint());
    emit modified();
    update();
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
        emit modified();
        update();
    }
}

void BezierSplineEditor::setPixmap(const QPixmap& pixmap)
{
    m_pixmap = pixmap;
    m_pixmapIsDirty = true;
    update();
}

void BezierSplineEditor::slotZoomIn()
{
    m_zoomLevel = qMax(m_zoomLevel-1, 0);
    m_pixmapIsDirty = true;
    update();
}

void BezierSplineEditor::slotZoomOut()
{
    m_zoomLevel = qMin(m_zoomLevel+1, 3);
    m_pixmapIsDirty = true;
    update();
}

int BezierSplineEditor::gridLines()
{
    return m_gridLines;
}

void BezierSplineEditor::setGridLines(int lines)
{
    m_gridLines = qBound(0, lines, 8);
    update();
}

void BezierSplineEditor::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);

    int wWidth = width() - 1;
    int wHeight = height() - 1;
    int offset = 1/8. * m_zoomLevel * (wWidth > wHeight ? wWidth : wHeight);
    wWidth -= 2 * offset;
    wHeight -= 2 * offset;

    p.translate(offset, offset);

    /*
     * Background
     */
    p.fillRect(rect().translated(-offset, -offset), palette().background());
    if (!m_pixmap.isNull()) {
        if (m_pixmapIsDirty || !m_pixmapCache) {
            if (m_pixmapCache)
                delete m_pixmapCache;
            m_pixmapCache = new QPixmap(wWidth + 1, wHeight + 1);
            QPainter cachePainter(m_pixmapCache);

            cachePainter.scale(1.0*(wWidth+1) / m_pixmap.width(), 1.0*(wHeight+1) / m_pixmap.height());
            cachePainter.drawPixmap(0, 0, m_pixmap);
            m_pixmapIsDirty = false;
        }
        p.drawPixmap(0, 0, *m_pixmapCache);
    }


    p.setPen(QPen(Qt::gray, 1, Qt::SolidLine));

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
        p.drawLine(QLineF(prevX, prevY, x, curY));
        prevX = x;
        prevY = curY;
    }
    p.drawLine(QLineF(prevX, prevY ,
                      x, wHeight - m_spline.value(1.0, true) * wHeight));

    /*
     * Points + Handles
     */
    int max = m_spline.points().count() - 1;
    p.setPen(QPen(Qt::red, 1, Qt::SolidLine));
    BPoint point;
    QPolygon handle(4);
    handle.setPoints(4,
                     1,  -2,
                     4,  1,
                     1,  4,
                     -2, 1);
#if QT_VERSION < 0x040600
    QPolygon tmp;
#endif
    for (int i = 0; i <= max; ++i) {
        point = m_spline.points().at(i);
        if (i == m_currentPointIndex) {
            // selected point: fill p and handles
            p.setBrush(QBrush(QColor(Qt::red), Qt::SolidPattern));
            // connect p and handles with lines
            if (i != 0)
                p.drawLine(QLineF(point.h1.x() * wWidth, wHeight - point.h1.y() * wHeight, point.p.x() * wWidth, wHeight - point.p.y() * wHeight));
            if (i != max)
                p.drawLine(QLineF(point.p.x() * wWidth, wHeight - point.p.y() * wHeight, point.h2.x() * wWidth, wHeight - point.h2.y() * wHeight));
        }

        p.drawEllipse(QRectF(point.p.x() * wWidth - 3,
                             wHeight - 3 - point.p.y() * wHeight, 6, 6));
        if (i != 0) {
#if QT_VERSION >= 0x040600
            p.drawConvexPolygon(handle.translated(point.h1.x() * wWidth, wHeight - point.h1.y() * wHeight));
#else
            tmp = handle;
            tmp.translate(point.h1.x() * wWidth, wHeight - point.h1.y() * wHeight);
            p.drawConvexPolygon(tmp);
#endif
        }
        if (i != max) {
#if QT_VERSION >= 0x040600
            p.drawConvexPolygon(handle.translated(point.h2.x() * wWidth, wHeight - point.h2.y() * wHeight));
#else
            tmp = handle;
            tmp.translate(point.h2.x() * wWidth, wHeight - point.h2.y() * wHeight);
            p.drawConvexPolygon(tmp);
#endif
        }

        if ( i == m_currentPointIndex)
            p.setBrush(QBrush(Qt::NoBrush));
    }
}

void BezierSplineEditor::resizeEvent(QResizeEvent* event)
{
    m_spline.setPrecision(width() > height() ? width() : height());
    m_pixmapIsDirty = true;
    QWidget::resizeEvent(event);
}

void BezierSplineEditor::mousePressEvent(QMouseEvent* event)
{
    int wWidth = width() - 1;
    int wHeight = height() - 1;
    int offset = 1/8. * m_zoomLevel * (wWidth > wHeight ? wWidth : wHeight);
    wWidth -= 2 * offset;
    wHeight -= 2 * offset;

    double x = (event->pos().x() - offset) / (double)(wWidth);
    double y = 1.0 - (event->pos().y() - offset) / (double)(wHeight);

    point_types selectedPoint;
    int closestPointIndex = nearestPointInRange(QPointF(x, y), wWidth, wHeight, &selectedPoint);

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
        m_currentPointIndex = m_spline.addPoint(BPoint(QPointF(x-0.05, y-0.05),
                                                       QPointF(x, y),
                                                       QPointF(x+0.05, y+0.05)));
        m_currentPointType = PTypeP;
    } else {
        m_currentPointIndex = closestPointIndex;
        m_currentPointType = selectedPoint;
    }

    BPoint point = m_spline.points()[m_currentPointIndex];

    m_grabPOriginal = point;
    if (m_currentPointIndex > 0)
        m_grabPPrevious = m_spline.points()[m_currentPointIndex - 1];
    if (m_currentPointIndex < m_spline.points().count() - 1)
        m_grabPNext = m_spline.points()[m_currentPointIndex + 1];
    m_grabOffsetX = point[(int)m_currentPointType].x() - x;
    m_grabOffsetY = point[(int)m_currentPointType].y() - y;

    point[(int)m_currentPointType] = QPointF(x + m_grabOffsetX, y + m_grabOffsetY);

    m_spline.setPoint(m_currentPointIndex, point);

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
    int wWidth = width() - 1;
    int wHeight = height() - 1;
    int offset = 1/8. * m_zoomLevel * (wWidth > wHeight ? wWidth : wHeight);
    wWidth -= 2 * offset;
    wHeight -= 2 * offset;

    double x = (event->pos().x() - offset) / (double)(wWidth);
    double y = 1.0 - (event->pos().y() - offset) / (double)(wHeight);
    
    if (m_mode == ModeNormal) {
        // If no point is selected set the the cursor shape if on top
        point_types type;
        int nearestPointIndex = nearestPointInRange(QPointF(x, y), wWidth, wHeight, &type);

        if (nearestPointIndex < 0)
            setCursor(Qt::ArrowCursor);
        else
            setCursor(Qt::CrossCursor);
    } else {
        // Else, drag the selected point
        setCursor(Qt::CrossCursor);

        x += m_grabOffsetX;
        y += m_grabOffsetY;

        double leftX = 0.;
        double rightX = 1.;
        BPoint point = m_spline.points()[m_currentPointIndex];
        switch (m_currentPointType) {
        case PTypeH1:
            rightX = point.p.x();
            if (m_currentPointIndex == 0)
                leftX = -4;
            else
                leftX = m_spline.points()[m_currentPointIndex - 1].p.x();

            x = qBound(leftX, x, rightX);
            point.setH1(QPointF(x, y));
            break;

        case PTypeP:
            if (m_currentPointIndex == 0)
                rightX = 0.0;
            else if (m_currentPointIndex == m_spline.points().count() - 1)
                leftX = 1.0;

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
            if (m_currentPointIndex == m_spline.points().count() - 1)
                rightX = 5;
            else
                rightX = m_spline.points()[m_currentPointIndex + 1].p.x();

            x = qBound(leftX, x, rightX);
            point.setH2(QPointF(x, y));
        };

        int index = m_currentPointIndex;
        m_currentPointIndex = m_spline.setPoint(m_currentPointIndex, point);

        if (m_currentPointType == PTypeP) {
            // we might have changed the handles of other points
            // try to restore
            if (index == m_currentPointIndex) {
                if (m_currentPointIndex > 0)
                    m_spline.setPoint(m_currentPointIndex - 1, m_grabPPrevious);
                if (m_currentPointIndex < m_spline.points().count() -1)
                    m_spline.setPoint(m_currentPointIndex + 1, m_grabPNext);
            } else {
                if (m_currentPointIndex < index) {
                    m_spline.setPoint(index, m_grabPPrevious);
                    m_grabPNext = m_grabPPrevious;
                    if (m_currentPointIndex > 0)
                        m_grabPPrevious = m_spline.points()[m_currentPointIndex - 1];
                } else {
                    m_spline.setPoint(index, m_grabPNext);
                    m_grabPPrevious = m_grabPNext;
                    if (m_currentPointIndex < m_spline.points().count() - 1)
                        m_grabPNext = m_spline.points()[m_currentPointIndex + 1];
                }
            }
        }

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
    // find out distance using the Pythagorean theorem
    foreach(const BPoint & point, m_spline.points()) {
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

    if (nearestIndex >= 0) {
        BPoint point = m_spline.points()[nearestIndex];
        if (qAbs(p.x() - point[(int)selectedPoint].x()) * wWidth < 5 && qAbs(p.y() - point[(int)selectedPoint].y()) * wHeight < 5) {
            *sel = selectedPoint;
            return nearestIndex;
        }
    }

    return -1;
}

#include "beziersplineeditor.moc"
