
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


#ifndef ABSTRACTPOINT_H
#define ABSTRACTPOINT_H

#include <QPointF>

/** @brief Base class for a curve point

 */
class AbstractPoint
{
public:
    /** @brief Sets the point to -1, -1  */
    AbstractPoint();
    /** @brief Sets up according to the params. */
    AbstractPoint(const QPointF &handle1, const QPointF &point, const QPointF &handle2);
    AbstractPoint(const QPointF &point);

    bool operator==(const AbstractPoint &point) const;
/** handle 1 */
    QPointF h1;
/** point */
    QPointF p;
/** handle 2 */
    QPointF h2;

    bool handlesLinked;
};


#endif
