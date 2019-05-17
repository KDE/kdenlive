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
#include "snapmodel.hpp"
#include <QDebug>
#include <climits>
#include <cstdlib>


SnapInterface::SnapInterface() = default;
SnapInterface::~SnapInterface() = default;

SnapModel::SnapModel() = default;

void SnapModel::addPoint(int position)
{
    if (m_snaps.count(position) == 0) {
        m_snaps[position] = 1;
    } else {
        m_snaps[position]++;
    }
}

void SnapModel::removePoint(int position)
{
    Q_ASSERT(m_snaps.count(position) > 0);
    if (m_snaps[position] == 1) {
        m_snaps.erase(position);
    } else {
        m_snaps[position]--;
    }
}

int SnapModel::getClosestPoint(int position)
{
    if (m_snaps.empty()) {
        return -1;
    }
    auto it = m_snaps.lower_bound(position);
    long long int prev = INT_MIN, next = INT_MAX;
    if (it != m_snaps.end()) {
        next = (*it).first;
    }
    if (it != m_snaps.begin()) {
        --it;
        prev = (*it).first;
    }
    if (std::llabs((long long)position - prev) < std::llabs((long long)position - next)) {
        return (int)prev;
    }
    return (int)next;
}

int SnapModel::getNextPoint(int position)
{
    if (m_snaps.empty()) {
        return position;
    }
    auto it = m_snaps.lower_bound(position + 1);
    long long int next = position;
    if (it != m_snaps.end()) {
        next = (*it).first;
    }
    return (int)next;
}

int SnapModel::getPreviousPoint(int position)
{
    if (m_snaps.empty()) {
        return 0;
    }
    auto it = m_snaps.lower_bound(position);
    long long int prev = 0;
    if (it != m_snaps.begin()) {
        --it;
        prev = (*it).first;
    }
    return (int)prev;
}

void SnapModel::ignore(const std::vector<int> &pts)
{
    for (int pt : pts) {
        removePoint(pt);
        m_ignore.push_back(pt);
    }
}

void SnapModel::unIgnore()
{
    for (const auto &pt : m_ignore) {
        addPoint(pt);
    }
    m_ignore.clear();
}

int SnapModel::proposeSize(int in, int out, int size, bool right, int maxSnapDist)
{
    ignore({in, out});
    int proposed_size = -1;
    if (right) {
        int target_pos = in + size - 1;
        int snapped_pos = getClosestPoint(target_pos);
        if (snapped_pos != -1 && qAbs(target_pos - snapped_pos) <= maxSnapDist) {
            proposed_size = snapped_pos - in;
        }
    } else {
        int target_pos = out + 1 - size;
        int snapped_pos = getClosestPoint(target_pos);
        if (snapped_pos != -1 && qAbs(target_pos - snapped_pos) <= maxSnapDist) {
            proposed_size = out - snapped_pos;
        }
    }
    unIgnore();
    return proposed_size;
}

int SnapModel::proposeSize(int in, int out, const std::vector<int> boundaries, int size, bool right, int maxSnapDist)
{
    ignore(boundaries);
    int proposed_size = -1;
    if (right) {
        int target_pos = in + size - 1;
        int snapped_pos = getClosestPoint(target_pos);
        if (snapped_pos != -1 && qAbs(target_pos - snapped_pos) <= maxSnapDist) {
            proposed_size = snapped_pos - in;
        }
    } else {
        int target_pos = out + 1 - size;
        int snapped_pos = getClosestPoint(target_pos);
        if (snapped_pos != -1 && qAbs(target_pos - snapped_pos) <= maxSnapDist) {
            proposed_size = out - snapped_pos;
        }
    }
    unIgnore();
    return proposed_size;
}
