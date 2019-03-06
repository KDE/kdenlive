/*
 *  Copyright (c) 2010 Cyrille Berger <cberger@cberger.net>
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

#ifndef KIS_CUBIC_CURVE_H
#define KIS_CUBIC_CURVE_H

#include <QList>
#include <QVector>

class QPointF;

/**
 * Hold the data for a cubic curve.
 */
class KisCubicCurve
{
public:
    using Point_t = QPointF;
    KisCubicCurve();
    explicit KisCubicCurve(const QList<QPointF> &points);
    explicit KisCubicCurve(const QVector<QPointF> &points);
    KisCubicCurve(const KisCubicCurve &curve);
    ~KisCubicCurve();
    KisCubicCurve &operator=(const KisCubicCurve &curve);
    bool operator==(const KisCubicCurve &curve) const;

public:
    qreal value(qreal x) const;
    QList<QPointF> points() const;
    void setPoints(const QList<QPointF> &points);
    int setPoint(int idx, const QPointF &point);
    /**
     * Add a point to the curve, the list of point is always sorted.
     * @return the index of the inserted point
     */
    int addPoint(const QPointF &point);
    void removePoint(int idx);
    /** @brief Returns the number of points on the curve
     */
    int count() const;
    /** @brief Returns the point at @param ix.
     * @param ix Index of the point
     * @param normalisedWidth (default = 1) Will be multiplied will all x values to change the range from 0-1 into another one
     * @param normalisedHeight (default = 1) Will be multiplied will all y values to change the range from 0-1 into another one
     * @param invertHeight (default = false) true => y = 0 is at the very top
     */
    QPointF getPoint(int ix, int normalisedWidth = 1, int normalisedHeight = 1, bool invertHeight = false);

public:
    QVector<quint16> uint16Transfer(int size = 256) const;
    QVector<qreal> floatTransfer(int size = 256) const;

public:
    const QString toString() const;
    void fromString(const QString &);

private:
    struct Data;
    struct Private;
    Private *const d; // NOLINT
};

#endif
