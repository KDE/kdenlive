/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef CUBICBEZIERSPLINE_H
#define CUBICBEZIERSPLINE_H

#include "bpoint.h"

#include <QList>
#include <QString>

/** @class CubicBezierSpline
    @brief \@todo Describe class CubicBezierSpline
    @todo Describe class CubicBezierSpline
 */
class CubicBezierSpline
{

public:
    using Point_t = BPoint;
    explicit CubicBezierSpline();
    CubicBezierSpline(const CubicBezierSpline &spline);
    CubicBezierSpline &operator=(const CubicBezierSpline &spline);

    /** @brief Loads the points from the string @param spline.
     *
     * x, y values have to be separated with a ';'
     * handles and points with a '#'
     * and the nodes with a '|'
     * So that you get: h1x;h1y#px;py#h2x;h2y|h1x;h1y#... */
    void fromString(const QString &spline);
    /** @brief Returns the points stored in a string.
     *
     * x, y values have are separated with a ';'
     * handles and points with a '#'
     * and the nodes with a '|'
     * So that you get: h1x;h1y#px;py#h2x;h2y|h1x;h1y#... */
    QString toString() const;

    /** @brief Returns a list of the points defining the spline. */
    QList<BPoint> points() const;

    /** @brief Returns the number of points in the spline.*/
    int count() const;

    /** @brief Sets the point at index @param ix to @param point and returns its index (it might have changed during validation). */
    int setPoint(int ix, const BPoint &point);
    /** @brief Adds @param point and returns its index. */
    int addPoint(const BPoint &point);
    /** @brief Add a point based on a position @param point only
        This will try to compute relevant handles based on neihbouring points
        Return the index of the added point.
    */
    int addPoint(const QPointF &point);
    /** @brief Removes the point at @param ix. */
    void removePoint(int ix);

    /** @brief Returns the point at @param ix.
     * @param ix Index of the point
     * @param normalisedWidth (default = 1) Will be multiplied will all x values to change the range from 0-1 into another one
     * @param normalisedHeight (default = 1) Will be multiplied will all y values to change the range from 0-1 into another one
     * @param invertHeight (default = false) true => y = 0 is at the very top
     */
    BPoint getPoint(int ix, int normalisedWidth = 1, int normalisedHeight = 1, bool invertHeight = false);

    /** @brief Returns the closest point to @param p, represented by its index and type (center or handle)
     */
    std::pair<int, BPoint::PointType> closestPoint(const QPointF &p) const;

    QList<BPoint> getPoints() const;

private:
    void validatePoints();
    void keepSorted();
    int indexOf(const BPoint &p);

    QList<BPoint> m_points;
};

#endif
