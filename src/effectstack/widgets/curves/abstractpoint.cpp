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


#include "abstractpoint.h"

AbstractPoint::AbstractPoint():
    h1(-1, -1),
    p(-1, -1),
    h2(-1, -1)
{
}

AbstractPoint::AbstractPoint(const QPointF &handle1, const QPointF &point, const QPointF &handle2) :
    h1(handle1),
    p(point),
    h2(handle2)
{
}
AbstractPoint::AbstractPoint(const QPointF &point) :
    h1(-1, -1),
    p(point),
    h2(-1, -1)
{
}

bool AbstractPoint::operator==(const AbstractPoint &point) const
{
    return point.h1 == h1 &&
        point.p  == p  &&
        point.h2 == h2;
}
