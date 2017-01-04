/*
 *  Copyright (c) 2005 C. Boemann <cbo@boemann.dk>
 *  Copyright (c) 2009 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// Local includes.

#include "kis_curve_widget.h"

// C++ includes.

#include <cmath>
#include <cstdlib>

// Qt includes.

#include <QPixmap>
#include <QPainter>
#include <QPoint>
#include <QPen>
#include <QEvent>
#include <QRect>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>

#include <QSpinBox>

#define bounds(x,a,b) (x<a ? a : (x>b ? b :x))
#define MOUSE_AWAY_THRES 15
#define POINT_AREA       1E-4
#define CURVE_AREA       1E-4


//static bool pointLessThan(const QPointF &a, const QPointF &b);

KisCurveWidget::KisCurveWidget(QWidget *parent)
    : AbstractCurveWidget(parent)
{
    setObjectName(QStringLiteral("KisCurveWidget"));
    m_currentPointIndex = -1;
    m_readOnlyMode   = false;
    m_guideVisible   = false;
    m_pixmapIsDirty = true;
    m_pixmapCache = Q_NULLPTR;

    m_intIn = Q_NULLPTR;
    m_intOut = Q_NULLPTR;

    m_maxPoints = -1;

    setMouseTracking(true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(150, 150);
    setMaximumSize(1000, 1000);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    update();
    emit modified();

    setFocusPolicy(Qt::StrongFocus);
    m_grabOffsetX = 0;
    m_grabOffsetY = 0;
    m_grabOriginalX = 0;
    m_grabOriginalY = 0;
    m_draggedAwayPointIndex = 0;
    m_readOnlyMode = 0;
    m_guideVisible = 0;
    m_pixmapIsDirty = 0;
    m_pixmapCache = NULL;
    m_intIn = NULL;
    m_intOut = NULL;
    m_inOutMin = 0;
    m_inOutMax = 0;
    m_maxPoints = 0;
    m_curve = KisCubicCurve();
    connect(this, &KisCurveWidget::modified, this, &KisCurveWidget::syncIOControls);
}

KisCurveWidget::~KisCurveWidget()
{
}

QSize KisCurveWidget::sizeHint() const
{
    return QSize(500, 500);
}

void KisCurveWidget::setupInOutControls(QSpinBox *in, QSpinBox *out, int min, int max)
{
    m_intIn = in;
    m_intOut = out;

    if (!m_intIn || !m_intOut) {
        return;
    }

    m_inOutMin = min;
    m_inOutMax = max;

    m_intIn->setRange(m_inOutMin, m_inOutMax);
    m_intOut->setRange(m_inOutMin, m_inOutMax);

    connect(m_intIn, SIGNAL(valueChanged(int)), this, SLOT(inOutChanged(int)));
    connect(m_intOut, SIGNAL(valueChanged(int)), this, SLOT(inOutChanged(int)));
    syncIOControls();

}

void KisCurveWidget::inOutChanged(int)
{
    QPointF pt;

    Q_ASSERT(m_currentPointIndex >= 0);

    pt.setX(io2sp(m_intIn->value()));
    pt.setY(io2sp(m_intOut->value()));

    if (jumpOverExistingPoints(pt, m_currentPointIndex)) {
        m_curve.setPoint(m_currentPointIndex, pt);
        m_currentPointIndex = m_curve.points().indexOf(pt);
    } else {
        pt = m_curve.points().at(m_currentPointIndex);
    }

    m_intIn->blockSignals(true);
    m_intOut->blockSignals(true);

    m_intIn->setValue(sp2io(pt.x()));
    m_intOut->setValue(sp2io(pt.y()));

    m_intIn->blockSignals(false);
    m_intOut->blockSignals(false);

    update();
    emit modified();
}

void KisCurveWidget::reset(void)
{
    m_currentPointIndex = -1;
    m_guideVisible = false;

    update();
    emit modified();
}


void KisCurveWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        if (m_currentPointIndex > 0 && m_currentPointIndex < m_curve.points().count() - 1) {
            //x() find closest point to get focus afterwards
            double grab_point_x = m_curve.points().at(m_currentPointIndex).x();

            int left_of_currentPointIndex = m_currentPointIndex - 1;
            int right_of_currentPointIndex = m_currentPointIndex + 1;
            int new_currentPointIndex;

            if (fabs(m_curve.points().at(left_of_currentPointIndex).x() - grab_point_x) <
                    fabs(m_curve.points().at(right_of_currentPointIndex).x() - grab_point_x)) {
                new_currentPointIndex = left_of_currentPointIndex;
            } else {
                new_currentPointIndex = m_currentPointIndex;
            }
            m_curve.removePoint(m_currentPointIndex);
            m_currentPointIndex = new_currentPointIndex;
            setCursor(Qt::ArrowCursor);
            m_state = State_t::NORMAL;
        }
        update();
        emit modified();
    } else if (e->key() == Qt::Key_Escape && m_state != State_t::NORMAL) {
        m_curve.setPoint(m_currentPointIndex, QPointF(m_grabOriginalX, m_grabOriginalY));
        setCursor(Qt::ArrowCursor);
        m_state = State_t::NORMAL;

        update();
        emit modified();
    } else if ((e->key() == Qt::Key_A || e->key() == Qt::Key_Insert) && m_state == State_t::NORMAL) {
        /* FIXME: Lets user choose the hotkeys */
        addPointInTheMiddle();
    } else {
        QWidget::keyPressEvent(e);
    }
}

void KisCurveWidget::addPointInTheMiddle()
{
    QPointF pt(0.5, m_curve.value(0.5));

    if (!jumpOverExistingPoints(pt, -1)) {
        return;
    }

    m_currentPointIndex = m_curve.addPoint(pt);

    if (m_intIn) {
        m_intIn->setFocus(Qt::TabFocusReason);
    }
    update();
    emit modified();
}

void KisCurveWidget::resizeEvent(QResizeEvent *e)
{
    m_pixmapIsDirty = true;
    QWidget::resizeEvent(e);
}

void KisCurveWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    paintBackground(&p);

    // Draw curve.
    int x;
    QPolygonF poly;

    p.setPen(QPen(palette().text().color(), 1, Qt::SolidLine));
    poly.reserve(m_wWidth);
    for (x = 0; x < m_wWidth; ++x) {
        double normalizedX = double(x) / m_wWidth;
        double curY = m_wHeight - m_curve.value(normalizedX) * m_wHeight;
        poly.append(QPointF(x, curY));
    }
    poly.append(QPointF(x, m_wHeight - m_curve.value(1.0) * m_wHeight));
    p.drawPolyline(poly);

    // Drawing curve handles.
    if (!m_readOnlyMode) {
        for (int i = 0; i < m_curve.points().count(); ++i) {
            double curveX = m_curve.points().at(i).x();
            double curveY = m_curve.points().at(i).y();

            if (i == m_currentPointIndex) {
                p.setPen(QPen(Qt::red, 3, Qt::SolidLine));
                p.drawEllipse(QRectF(curveX * m_wWidth - 2,
                                     m_wHeight - 2 - curveY * m_wHeight, 4, 4));
            } else {
                p.setPen(QPen(Qt::red, 1, Qt::SolidLine));
                p.drawEllipse(QRectF(curveX * m_wWidth - 3,
                                     m_wHeight - 3 - curveY * m_wHeight, 6, 6));
            }
        }
    }
}

void KisCurveWidget::mousePressEvent(QMouseEvent *e)
{
    if (m_readOnlyMode) {
        return;
    }

    int wWidth = width() - 1;
    int wHeight = height() - 1;
    int offsetX = 1 / 8. * m_zoomLevel * wWidth;
    int offsetY = 1 / 8. * m_zoomLevel * wHeight;
    wWidth -= 2 * offsetX;
    wHeight -= 2 * offsetY;
    double x = (e->pos().x() - offsetX) / (double)(wWidth);
    double y = 1.0 - (e->pos().y() - offsetY) / (double)(wHeight);

    int closest_point_index = nearestPointInRange(QPointF(x, y), width(), height());

    if (e->button() == Qt::RightButton && closest_point_index > 0 && closest_point_index < m_curve.points().count() - 1) {
        m_curve.removePoint(closest_point_index);
        setCursor(Qt::ArrowCursor);
        m_state = State_t::NORMAL;
        if (closest_point_index < m_currentPointIndex) {
            --m_currentPointIndex;
        }
        update();
        emit modified();
        return;
    } else if (e->button() != Qt::LeftButton) {
        return;
    }

    if (closest_point_index < 0) {
        if (m_maxPoints > 0 && m_curve.points().count() >= m_maxPoints) {
            return;
        }
        QPointF newPoint(x, y);
        if (!jumpOverExistingPoints(newPoint, -1)) {
            return;
        }
        m_currentPointIndex = m_curve.addPoint(newPoint);
    } else {
        m_currentPointIndex = closest_point_index;
    }

    m_grabOriginalX = m_curve.points().at(m_currentPointIndex).x();
    m_grabOriginalY = m_curve.points().at(m_currentPointIndex).y();
    m_grabOffsetX = m_curve.points().at(m_currentPointIndex).x() - x;
    m_grabOffsetY = m_curve.points().at(m_currentPointIndex).y() - y;
    m_curve.setPoint(m_currentPointIndex, QPointF(x + m_grabOffsetX, y + m_grabOffsetY));

    m_draggedAwayPointIndex = -1;
    m_state = State_t::DRAG;

    update();
    emit modified();
}

void KisCurveWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_readOnlyMode) {
        return;
    }

    if (e->button() != Qt::LeftButton) {
        return;
    }

    setCursor(Qt::ArrowCursor);
    m_state = State_t::NORMAL;

    update();
    emit modified();
}

void KisCurveWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (m_readOnlyMode) {
        return;
    }

    int wWidth = width() - 1;
    int wHeight = height() - 1;
    int offsetX = 1 / 8. * m_zoomLevel * wWidth;
    int offsetY = 1 / 8. * m_zoomLevel * wHeight;
    wWidth -= 2 * offsetX;
    wHeight -= 2 * offsetY;
    double x = (e->pos().x() - offsetX) / (double)(wWidth);
    double y = 1.0 - (e->pos().y() - offsetY) / (double)(wHeight);

    if (m_state == State_t::NORMAL) { // If no point is selected set the cursor shape if on top
        int nearestPointIndex = nearestPointInRange(QPointF(x, y), width(), height());

        if (nearestPointIndex < 0) {
            setCursor(Qt::ArrowCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
    } else { // Else, drag the selected point
        bool crossedHoriz = e->pos().x() - width() > MOUSE_AWAY_THRES ||
                            e->pos().x() < -MOUSE_AWAY_THRES;
        bool crossedVert =  e->pos().y() - height() > MOUSE_AWAY_THRES ||
                            e->pos().y() < -MOUSE_AWAY_THRES;

        bool removePoint = (crossedHoriz || crossedVert);

        if (!removePoint && m_draggedAwayPointIndex >= 0) {
            // point is no longer dragged away so reinsert it
            QPointF newPoint(m_draggedAwayPoint);
            m_currentPointIndex = m_curve.addPoint(newPoint);
            m_draggedAwayPointIndex = -1;
        }

        if (removePoint &&
                (m_draggedAwayPointIndex >= 0)) {
            return;
        }

        setCursor(Qt::CrossCursor);

        x += m_grabOffsetX;
        y += m_grabOffsetY;

        double leftX;
        double rightX;
        if (m_currentPointIndex == 0) {
            leftX = 0.0;
            rightX = 0.0;
            /*if (m_curve.points().count() > 1)
                rightX = m_curve.points().at(m_currentPointIndex + 1).x() - POINT_AREA;
            else
                rightX = 1.0;*/
        } else if (m_currentPointIndex == m_curve.points().count() - 1) {
            leftX = m_curve.points().at(m_currentPointIndex - 1).x() + POINT_AREA;
            rightX = 1.0;
        } else {
            Q_ASSERT(m_currentPointIndex > 0 && m_currentPointIndex < m_curve.points().count() - 1);

            // the 1E-4 addition so we can grab the dot later.
            leftX = m_curve.points().at(m_currentPointIndex - 1).x() + POINT_AREA;
            rightX = m_curve.points().at(m_currentPointIndex + 1).x() - POINT_AREA;
        }

        x = bounds(x, leftX, rightX);
        y = bounds(y, 0., 1.);

        m_curve.setPoint(m_currentPointIndex, QPointF(x, y));

        if (removePoint && m_curve.points().count() > 2) {
            m_draggedAwayPoint = m_curve.points().at(m_currentPointIndex);
            m_draggedAwayPointIndex = m_currentPointIndex;
            m_curve.removePoint(m_currentPointIndex);
            m_currentPointIndex = bounds(m_currentPointIndex, 0, m_curve.points().count() - 1);
        }

        update();
        emit modified();
    }
}


void KisCurveWidget::leaveEvent(QEvent *)
{
}

double KisCurveWidget::io2sp(int x) const
{
    int rangeLen = m_inOutMax - m_inOutMin;
    return double(x - m_inOutMin) / rangeLen;
}

int KisCurveWidget::sp2io(double x) const
{
    int rangeLen = m_inOutMax - m_inOutMin;
    return int(x * rangeLen + 0.5) + m_inOutMin;
}

bool KisCurveWidget::jumpOverExistingPoints(QPointF &pt, int skipIndex)
{
    foreach (const QPointF &it, m_curve.points()) {
        if (m_curve.points().indexOf(it) == skipIndex) {
            continue;
        }
        if (fabs(it.x() - pt.x()) < POINT_AREA)
            pt.rx() = pt.x() >= it.x() ?
                      it.x() + POINT_AREA : it.x() - POINT_AREA;
    }
    return (pt.x() >= 0 && pt.x() <= 1.);
}

int KisCurveWidget::nearestPointInRange(QPointF pt, int wWidth, int wHeight) const
{
    double nearestDistanceSquared = 1000;
    int nearestIndex = -1;
    int i = 0;

    foreach (const QPointF &point, m_curve.points()) {
        double distanceSquared = (pt.x() - point.x()) *
                                 (pt.x() - point.x()) +
                                 (pt.y() - point.y()) *
                                 (pt.y() - point.y());

        if (distanceSquared < nearestDistanceSquared) {
            nearestIndex = i;
            nearestDistanceSquared = distanceSquared;
        }
        ++i;
    }

    if (nearestIndex >= 0) {
        if (fabs(pt.x() - m_curve.points().at(nearestIndex).x()) * (wWidth - 1) < 5 &&
                fabs(pt.y() - m_curve.points().at(nearestIndex).y()) * (wHeight - 1) < 5) {
            return nearestIndex;
        }
    }

    return -1;
}


void KisCurveWidget::syncIOControls()
{
    if (!m_intIn || !m_intOut) {
        return;
    }

    bool somethingSelected = (m_currentPointIndex >= 0);

    m_intIn->setEnabled(somethingSelected);
    m_intOut->setEnabled(somethingSelected);

    if (m_currentPointIndex >= 0) {
        m_intIn->blockSignals(true);
        m_intOut->blockSignals(true);

        m_intIn->setValue(sp2io(m_curve.points().at(m_currentPointIndex).x()));
        m_intOut->setValue(sp2io(m_curve.points().at(m_currentPointIndex).y()));

        m_intIn->blockSignals(false);
        m_intOut->blockSignals(false);
    } else {
        /*FIXME: Ideally, these controls should hide away now */
    }
}
void KisCurveWidget::setCurve(KisCubicCurve&& curve)
{
    m_curve = std::move(curve);
}



QList<QPointF> KisCurveWidget::getPoints() const
{
    return m_curve.points();
}
