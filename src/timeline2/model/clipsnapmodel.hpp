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

#include "snapmodel.hpp"

#include <map>
#include <memory>
#include <unordered_set>

class MarkerListModel;

/** @brief This class represents the snap points of a clip of the timeline.
    Basically, one can add or remove snap points
 *
 */

class ClipSnapModel : public virtual SnapInterface, public std::enable_shared_from_this<SnapInterface>
{
public:
    ClipSnapModel();

    /* @brief Adds a snappoint at given position */
    void addPoint(int position) override;

    /* @brief Removes a snappoint from given position */
    void removePoint(int position) override;

    void registerSnapModel(const std::weak_ptr<SnapModel> &snapModel, int position, int in, int out, double speed = 1.);
    void deregisterSnapModel();

    void setReferenceModel(const std::weak_ptr<MarkerListModel> &markerModel, double speed);

    void updateSnapModelPos(int newPos);
    void updateSnapModelInOut(std::pair<int, int> newInOut);
    /* @brief Retrieve all snap points */
    void allSnaps(std::vector<int> &snaps, int offset = 0);


private:
    std::weak_ptr<SnapModel> m_registeredSnap;
    std::weak_ptr<MarkerListModel> m_parentModel;
    std::unordered_set<int> m_snapPoints;
    int m_inPoint;
    int m_outPoint;
    int m_position;
    double m_speed{1.};
    void addAllSnaps();
    void removeAllSnaps();

};

#endif
