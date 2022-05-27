/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
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
    if (std::llabs(position - prev) < std::llabs(position - next)) {
        return int(prev);
    }
    return int(next);
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
    return int(next);
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
    return int(prev);
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

int SnapModel::proposeSize(int in, int out, const std::vector<int> &boundaries, int size, bool right, int maxSnapDist)
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
