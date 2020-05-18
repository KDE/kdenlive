/***************************************************************************
 *   Copyright (C) 2019 by Jean-Baptiste Mardelle                          *
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

#include "bin/model/markerlistmodel.hpp"
#include "clipsnapmodel.hpp"
#include <climits>
#include <cstdlib>
#include <memory>

ClipSnapModel::ClipSnapModel() = default;

void ClipSnapModel::addPoint(int position)
{
    m_snapPoints.insert(position);
    if (position < m_inPoint * m_speed || position >= m_outPoint * m_speed) {
        return;
    }
    if (auto ptr = m_registeredSnap.lock()) {
        ptr->addPoint(m_speed < 0 ? ceil(m_outPoint + m_position + position / m_speed - m_inPoint) : ceil(m_position + position / m_speed - m_inPoint));
    }
}

void ClipSnapModel::removePoint(int position)
{
    m_snapPoints.erase(position);
    if (position < m_inPoint * m_speed || position >= m_outPoint * m_speed) {
        return;
    }
    if (auto ptr = m_registeredSnap.lock()) {
        ptr->removePoint(m_speed < 0 ? ceil(m_outPoint + m_position + position / m_speed - m_inPoint) : ceil(m_position + position / m_speed - m_inPoint));
    }
}

void ClipSnapModel::updateSnapModelPos(int newPos)
{
    if (newPos == m_position) {
        return;
    }
    removeAllSnaps();
    m_position = newPos;
    addAllSnaps();
}

void ClipSnapModel::updateSnapModelInOut(std::pair<int, int> newInOut)
{
    removeAllSnaps();
    m_inPoint = newInOut.first;
    m_outPoint = newInOut.second;
    addAllSnaps();
}

void ClipSnapModel::addAllSnaps()
{
    if (auto ptr = m_registeredSnap.lock()) {
        for (const auto &snap : m_snapPoints) {
            if (snap >= m_inPoint * m_speed && snap < m_outPoint * m_speed) {
                ptr->addPoint(m_speed < 0 ? ceil(m_outPoint + m_position + snap / m_speed - m_inPoint) : ceil(m_position + snap / m_speed - m_inPoint));
            }
        }
    }
}

void ClipSnapModel::removeAllSnaps()
{
    if (auto ptr = m_registeredSnap.lock()) {
        for (const auto &snap : m_snapPoints) {
            if (snap >= m_inPoint * m_speed && snap < m_outPoint * m_speed) {
                ptr->removePoint(m_speed < 0 ? ceil(m_outPoint + m_position + snap / m_speed - m_inPoint) : ceil(m_position + snap / m_speed - m_inPoint));
            }
        }
    }
}

void ClipSnapModel::allSnaps(std::vector<int> &snaps, int offset)
{
    snaps.push_back(m_position - offset);
    if (auto ptr = m_registeredSnap.lock()) {
        for (const auto &snap : m_snapPoints) {
            if (snap >= m_inPoint * m_speed && snap < m_outPoint * m_speed) {
                snaps.push_back(m_speed < 0 ? ceil(m_outPoint + m_position + snap / m_speed - m_inPoint - offset) : ceil(m_position + snap / m_speed - m_inPoint - offset));
            }
        }
    }
    snaps.push_back(m_position + m_outPoint - m_inPoint + 1 - offset);
}

void ClipSnapModel::registerSnapModel(const std::weak_ptr<SnapModel> &snapModel, int position, int in, int out, double speed)
{
    // make sure ptr is valid
    m_inPoint = in;
    m_outPoint = out;
    m_speed = speed;
    m_position = qMax(0, position);
    m_registeredSnap = snapModel;
    addAllSnaps();
}

void ClipSnapModel::deregisterSnapModel()
{
    // make sure ptr is valid
    removeAllSnaps();
    m_registeredSnap.reset();
}

void ClipSnapModel::setReferenceModel(const std::weak_ptr<MarkerListModel> &markerModel, double speed)
{
    m_parentModel = markerModel;
    m_speed = speed;
    if (auto ptr = m_parentModel.lock()) {
        ptr->registerSnapModel(std::static_pointer_cast<SnapInterface>(shared_from_this()));
    }
}
