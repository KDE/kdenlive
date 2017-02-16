/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef SNAPMODEL_H
#define SNAPMODEL_H

#include <map>

/** @brief This class represents the snap points of the timeline.
    Basically, one can add or remove snap points, and query the closest snap point to a given location
 *
 */

class SnapModel
{
public:
    SnapModel();

    /* @brief Adds a snappoint at given position */
    void addPoint(int position);

    /* @brief Removes a snappoint from given position */
    void removePoint(int position);

    /* @brief Retrieves closest point. Returns -1 if there is no snappoint available */
    int getClosestPoint(int position);

private:
    std::map<int, int> m_snaps; //This represents the snappoints internally. The keys are the positions and the values are the number of elements at this position. Note that it is important that the datastructure is ordered. QMap is NOT ordered, and therefore not suitable.
};

#endif
