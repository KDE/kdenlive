/***************************************************************************
 *   Copyright (C) 2019 by Jean-Baptiste Mardelle                                  *
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

#ifndef CLIPSNAPMODEL_H
#define CLIPSNAPMODEL_H

#include <map>
#include <vector>
#include <memory>

class SnapModel;
class MarkerListModel;

/** @brief This class represents the snap points of a clip of the timeline.
    Basically, one can add or remove snap points, and query the closest snap point to a given location
 *
 */

class ClipSnapModel : public enable_shared_from_this_virtual<ClipSnapModel>
{
public:
    ClipSnapModel();

    /* @brief Adds a snappoint at given position */
    void addPoint(size_t position);

    /* @brief Removes a snappoint from given position */
    void removePoint(size_t position);

    void registerSnapModel(const std::weak_ptr<SnapModel> &snapModel, size_t position, size_t in, size_t out);
    void unregisterSnapModel();
    
    void setReferenceModel(const std::weak_ptr<MarkerListModel> &markerModel);


private:
    std::weak_ptr<SnapModel> m_registeredSnap;
    std::weak_ptr<MarkerListModel> m_parentModel;
    size_t m_inPoint;
    size_t m_outPoint;
    size_t m_position;

};

#endif
