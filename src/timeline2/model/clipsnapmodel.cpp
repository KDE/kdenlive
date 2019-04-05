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
#include "snapmodel.hpp"
#include <QDebug>
#include <climits>
#include <cstdlib>
#include <memory>

ClipSnapModel::ClipSnapModel() = default;

void ClipSnapModel::addPoint(size_t position)
{
    if (position <= m_inPoint || position >= m_outPoint) {
        return;
    }
    if (auto ptr = m_registeredSnap.lock()) {        
        ptr->addPoint(m_position + position - m_inPoint);
    }
}

void ClipSnapModel::removePoint(size_t position)
{
    if (position <= m_inPoint || position >= m_outPoint) {
        return;
    }
    if (auto ptr = m_registeredSnap.lock()) {
        ptr->removePoint(m_position + position - m_inPoint);
    }
}

void ClipSnapModel::registerSnapModel(const std::weak_ptr<SnapModel> &snapModel, size_t position, size_t in, size_t out)
{
    // make sure ptr is valid
    m_inPoint = in;
    m_outPoint = out;
    m_position = position;
    auto ptr = m_registeredSnap.lock();
    if (!ptr) {
        m_registeredSnap = snapModel;
        ptr = m_registeredSnap.lock();
    }
    if (ptr) {
        // we now add the already existing markers to the snap
        if (auto ptr2 = m_parentModel.lock()) {
        std::vector<size_t> snaps = ptr2->getSnapPoints();
            for (const auto &snap : snaps) {
                if (snap < m_inPoint) {
                    continue;
                }
                if (snap > m_outPoint) {
                    break;
                }
                ptr->addPoint(m_position + snap - m_inPoint);
            }
        }
    } else {
        qDebug() << "Error: added snapmodel is null";
        Q_ASSERT(false);
    }
}

void ClipSnapModel::unregisterSnapModel()
{
    // make sure ptr is valid
    auto ptr = m_registeredSnap.lock();
    if (!ptr) {
        return;
    }
    // we now add the already existing markers to the snap
    if (auto ptr2 = m_parentModel.lock()) {
        std::vector<size_t> snaps = ptr2->getSnapPoints();
        for (const auto &snap : snaps) {
            if (snap < m_inPoint) {
                continue;
            }
            if (snap > m_outPoint) {
                break;
            }
            ptr->removePoint(m_position + snap - m_inPoint);
        }
    }
}

void ClipSnapModel::setReferenceModel(const std::weak_ptr<MarkerListModel> &markerModel)
{
    m_parentModel = markerModel;
    if (auto ptr = m_parentModel.lock()) {
        ptr->registerClipSnapModel(shared_from_this());
    }
}
