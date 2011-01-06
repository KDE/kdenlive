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

#ifndef CUBICBEZIERSPLINE_H
#define CUBICBEZIERSPLINE_H

#include "bpoint.h"

#include <QtCore>


class CubicBezierSpline : public QObject
{
    Q_OBJECT

public:
    CubicBezierSpline(QObject* parent = 0);
    CubicBezierSpline(const CubicBezierSpline &spline, QObject* parent = 0);
    CubicBezierSpline& operator=(const CubicBezierSpline &spline);

    /** @brief Loads the points from the string @param spline.
     *
     * x, y values have to be separated with a ';'
     * handles and points with a '#'
     * and the nodes with a '|'
     * So that you get: h1x;h1y#px;py#h2x;h2y|h1x;h1y#... */
    void fromString(const QString &spline);
    /** @brief Returns the points stoed in a string.
     *
     * x, y values have are separated with a ';'
     * handles and points with a '#'
     * and the nodes with a '|'
     * So that you get: h1x;h1y#px;py#h2x;h2y|h1x;h1y#... */
    QString toString() const;

    /** @brief Returns a list of the points defining the spline. */
    QList <BPoint> points();
    /** @brief Finds the closest point in the pre-calculated spline to @param x.
     * @param x x value
     * @param cont (default = false) Whether to start searching at the previously requested x and skip all values before it.
                                     This makes requesting a lot of increasing x's faster. */
    qreal value(qreal x, bool cont = false);

    /** @brief Sets the point at index @param ix to @param point and returns its index (it might have changed during validation). */
    int setPoint(int ix, const BPoint &point);
    /** @brief Adds @param point and returns its index. */
    int addPoint(const BPoint &point);
    /** @brief Removes the point at @param ix. */
    void removePoint(int ix);

    /** @brief Sets the precision to @param pre.
     *
     * The precision influences the number of points that are calculated for the spline:
     * number of values = precision * 10 */
    void setPrecision(int pre);
    int getPrecision();

private:
    QPointF point(double t, const QList<QPointF> &points);
    void validatePoints();
    void keepSorted();
    void update();
    int indexOf(const BPoint &p);

    QList <BPoint> m_points;
    /** x, y pairs */
    QMap <double, double> m_spline;
    /** iterator used when searching for a value in in continue mode (see @function value). */
    QMap <double, double>::const_iterator m_i;
    /** Whether the spline represents the points and the precision. */
    bool m_validSpline;
    int m_precision;
};

#endif
