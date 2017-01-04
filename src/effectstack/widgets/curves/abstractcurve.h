/***************************************************************************
 *   Copyright (C) 2016 by Nicolas Carion                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef ABSTRACTCURVE_H
#define ABSTRACTCURVE_H

#include <QWidget>
class QPainter;
/** @brief Base class of all the objects representing a curve of points

 */
template<typename Point_t>
class AbstractCurve
{
public:
    virtual ~AbstractCurve(){};

    AbstractCurve(){};

    /** @brief Constructs the curve from @param string
     */
    virtual void fromString(const QString &string) = 0;

    /** @brief Returns a string corresponding to the curve
     */
    virtual QString toString() const = 0;


    /** @brief Returns the number of points on the curve
     */
    virtual int count() const = 0;

    /** @brief Returns the point at @param ix.
     * @param ix Index of the point
     * @param normalisedWidth (default = 1) Will be multiplied will all x values to change the range from 0-1 into another one
     * @param normalisedHeight (default = 1) Will be multiplied will all y values to change the range from 0-1 into another one
     * @param invertHeight (default = false) true => y = 0 is at the very top
     */
    virtual Point_t getPoint(int ix, int normalisedWidth = 1, int normalisedHeight = 1, bool invertHeight = false) = 0;

    /** @brief Adds @param point and returns its index. */
    virtual int addPoint(const Point_t &point) = 0;
    /** @brief Removes the point at @param ix. */
    virtual void removePoint(int ix) = 0;
    /** @brief Sets the point at index @param ix to @param point and returns its index (it might have changed during validation). */
    virtual int setPoint(int ix, const Point_t &point) = 0;
};


#endif
