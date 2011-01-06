/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
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

#ifndef BPOINT_H
#define BPOINT_H

#include <QtCore>

/**
 * @brief Represents a point in a cubic Bézier spline.
 */

class BPoint
{
public:
    /** @brief Sets the point to -1, -1 to mark it as unusable (until point + handles have proper values) */
    BPoint();
    /** @brief Sets up according to the params. Linking detecting is done using autoSetLinked(). */
    BPoint(QPointF handle1, QPointF point, QPointF handle2);

    /** @brief Returns h1 if i = 0, p if i = 1, h2 if i = 2. */
    QPointF &operator[](int i);
    /** @brief Returns h1 if i = 0, p if i = 1, h2 if i = 2. */
    const QPointF &operator[](int i) const;
    bool operator==(const BPoint &point) const;

    /** @brief Sets p to @param point.
     * @param updateHandles (default = true) Whether to make sure the handles keep their position relative to p. */
    void setP(QPointF point, bool updateHandles = true);

    /** @brief Sets h1 to @param handle1.
     *
     * If handlesLinked is true h2 is updated. */
    void setH1(QPointF handle1);

    /** @brief Sets h2 to @param handle2.
     * 
     * If handlesLinked is true h1 is updated. */
    void setH2(QPointF handle2);
    void keepInRange(qreal xMin, qreal xMax);

    /** @brief Sets handlesLinked to true if the handles are in a linked state (line through h1, p, h2) otherwise to false. */
    void autoSetLinked();

    /** handle 1 */
    QPointF h1;
    /** point */
    QPointF p;
    /** handle 2 */
    QPointF h2;

    /** handles are linked to achieve a natural locking spline => PH1 = -r*PH2 ; a line can be drawn through h1, p, h2 */
    bool handlesLinked;
};

#endif
