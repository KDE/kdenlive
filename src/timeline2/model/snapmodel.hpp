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
#include <vector>

/** @brief This is a base class for snap models (timeline, clips)
    Implements only basic functions like add or remove snap points
 */

class SnapInterface
{
public:
    SnapInterface();
    virtual ~SnapInterface();
    /* @brief Adds a snappoint at given position */
    virtual void addPoint(int position) = 0;

    /* @brief Removes a snappoint from given position */
    virtual void removePoint(int position) = 0;
};


/** @brief This class represents the snap points of the timeline.
    Basically, one can add or remove snap points, and query the closest snap point to a given location
 *
 */

class SnapModel : public virtual SnapInterface
{
public:
    SnapModel();

    /* @brief Adds a snappoint at given position */
    void addPoint(int position) override;

    /* @brief Removes a snappoint from given position */
    void removePoint(int position) override;

    /* @brief Retrieves closest point. Returns -1 if there is no snappoint available */
    int getClosestPoint(int position);

    /* @brief Retrieves next snap point. Returns position if there is no snappoint available */
    int getNextPoint(int position);

    /* @brief Retrieves previous snap point. Returns 0 if there is no snappoint available */
    int getPreviousPoint(int position);

    /* @brief Ignores the given positions until unIgnore() is called
       You can make several call to this before unIgnoring
       Note that you cannot remove ignored points.
       @param points list of point to ignore
     */
    void ignore(const std::vector<int> &pts);

    /* @brief Revert ignoring
     */
    void unIgnore();

    /* @brief Propose a size for the item (clip, composition,...) being resized, based on the snap points.
       @param in current inpoint of the item
       @param out current outpoint of the item
       @param size is the size requested before snapping
       @param right true if we resize the right end of the item
       @param maxSnapDist maximal number of frames we are allowed to snap to
    */
    int proposeSize(int in, int out, int size, bool right, int maxSnapDist);
    int proposeSize(int in, int out, const std::vector<int> boundaries, int size, bool right, int maxSnapDist);

    // For testing only
    std::map<int, int> _snaps() { return m_snaps; }

private:
    std::map<int, int> m_snaps; // This represents the snappoints internally. The keys are the positions and the values are the number of elements at this
                                // position. Note that it is important that the datastructure is ordered. QMap is NOT ordered, and therefore not suitable.

    std::vector<int> m_ignore;
};

#endif
