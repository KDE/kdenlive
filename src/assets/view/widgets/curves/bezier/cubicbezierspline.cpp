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

#include "cubicbezierspline.h"
#include <cmath>

/** @brief For sorting a Bezier spline. Whether a is before b. */
static bool pointLessThan(const BPoint &a, const BPoint &b)
{
    return a.p.x() < b.p.x();
}

CubicBezierSpline::CubicBezierSpline()
{
    m_points.append(BPoint(QPointF(0, 0), QPointF(0, 0), QPointF(.1, .1)));
    m_points.append(BPoint(QPointF(.9, .9), QPointF(1, 1), QPointF(1, 1)));
}

CubicBezierSpline::CubicBezierSpline(const CubicBezierSpline &spline)
{
    m_points = spline.m_points;
}

CubicBezierSpline &CubicBezierSpline::operator=(const CubicBezierSpline &spline) = default;

void CubicBezierSpline::fromString(const QString &spline)
{
    m_points.clear();
    const QStringList bpoints = spline.split(QLatin1Char('|'));
    for (const QString &bpoint : bpoints) {
        const QStringList points = bpoint.split(QLatin1Char('#'));
        QVector<QPointF> values;
        for (const QString &point : points) {
            const QStringList xy = point.split(QLatin1Char(';'));
            if (xy.count() == 2) {
                values.append(QPointF(xy.at(0).toDouble(), xy.at(1).toDouble()));
            }
        }
        if (values.count() == 3) {
            m_points.append(BPoint(values.at(0), values.at(1), values.at(2)));
        }
    }

    keepSorted();
    validatePoints();
}

QString CubicBezierSpline::toString() const
{
    QStringList spline;
    for (const BPoint &p : m_points) {
        spline << QStringLiteral("%1;%2#%3;%4#%5;%6")
                .arg(QString::number(p.h1.x(), 'f'), QString::number(p.h1.y(), 'f'), QString::number(p.p.x(), 'f'),
                     QString::number(p.p.y(), 'f'), QString::number(p.h2.x(), 'f'),
                     QString::number(p.h2.y(), 'f'));
    }
    return spline.join(QLatin1Char('|'));
}

int CubicBezierSpline::setPoint(int ix, const BPoint &point)
{
    m_points[ix] = point;
    keepSorted();
    validatePoints();
    return indexOf(point); // in case it changed
}

QList<BPoint> CubicBezierSpline::points() const
{
    return m_points;
}

void CubicBezierSpline::removePoint(int ix)
{
    m_points.removeAt(ix);
}

int CubicBezierSpline::addPoint(const BPoint &point)
{
    m_points.append(point);
    keepSorted();
    validatePoints();
    return indexOf(point);
}

int CubicBezierSpline::addPoint(const QPointF &point)
{
    // Check if point is in range
    if (point.x() < m_points[0].p.x() || point.x() > m_points.back().p.x()) {
        return -1;
    }
    // first we find by dichotomy the previous and next points on the curve
    int prev = 0, next = m_points.size() - 1;
    while (prev < next - 1) {
        int mid = (prev + next) / 2;
        if (point.x() < m_points[mid].p.x()) {
            next = mid;
        } else {
            prev = mid;
        }
    }

    // compute vector between adjacent points
    QPointF vec = m_points[next].p - m_points[prev].p;
    // normalize
    vec /= 10. * sqrt(vec.x() * vec.x() + vec.y() * vec.y());
    // add resulting point
    return addPoint(BPoint(point - vec, point, point + vec));
}

BPoint CubicBezierSpline::getPoint(int ix, int normalisedWidth, int normalisedHeight, bool invertHeight)
{
    BPoint p = m_points.at(ix);
    for (int i = 0; i < 3; ++i) {
        p[i].rx() *= normalisedWidth;
        p[i].ry() *= normalisedHeight;
        if (invertHeight) {
            p[i].ry() = normalisedHeight - p[i].y();
        }
    }
    return p;
}

void CubicBezierSpline::validatePoints()
{
    BPoint p1, p2;
    for (int i = 0; i < m_points.count() - 1; ++i) {
        p1 = m_points.at(i);
        p2 = m_points.at(i + 1);
        p1.h2.setX(qBound(p1.p.x(), p1.h2.x(), p2.p.x()));
        p2.h1.setX(qBound(p1.p.x(), p2.h1.x(), p2.p.x()));
        m_points[i] = p1;
        m_points[i + 1] = p2;
    }
}

void CubicBezierSpline::keepSorted()
{
    std::sort(m_points.begin(), m_points.end(), pointLessThan);
}

int CubicBezierSpline::indexOf(const BPoint &p)
{
    if (m_points.indexOf(p) == -1) {
        // point changed during validation process
        for (int i = 0; i < m_points.count(); ++i) {
            // this condition should be sufficient, too
            if (m_points.at(i).p == p.p) {
                return i;
            }
        }
    } else {
        return m_points.indexOf(p);
    }
    return -1;
}

int CubicBezierSpline::count() const
{
    return m_points.size();
}

QList<BPoint> CubicBezierSpline::getPoints() const
{
    return m_points;
}

std::pair<int, BPoint::PointType> CubicBezierSpline::closestPoint(const QPointF &p) const
{
    double nearestDistanceSquared = 1e100;
    BPoint::PointType selectedPoint = BPoint::PointType::P;
    int nearestIndex = -1;
    int i = 0;

    // find out distance using the Pythagorean theorem
    for (const auto &point : m_points) {
        for (int j = 0; j < 3; ++j) {
            double dx = point[j].x() - p.x();
            double dy = point[j].y() - p.y();
            double distanceSquared = dx * dx + dy * dy;
            if (distanceSquared < nearestDistanceSquared) {
                nearestIndex = i;
                nearestDistanceSquared = distanceSquared;
                selectedPoint = (BPoint::PointType)j;
            }
        }
        ++i;
    }
    return {nearestIndex, selectedPoint};
}
