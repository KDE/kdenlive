/*
    SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QList>
#include <QVector>

class QPointF;

/** @class KisCubicCurve
    @brief Hold the data for a cubic curve.
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
