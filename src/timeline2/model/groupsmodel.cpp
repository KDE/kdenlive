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

#include "groupsmodel.hpp"
#include "timelinemodel.hpp"
#include <QDebug>
#include <queue>

GroupsModel::GroupsModel(std::weak_ptr<TimelineModel> parent) :
    m_parent(parent)
{
}

int GroupsModel::groupItems(const std::unordered_set<int>& ids)
{
    Q_ASSERT(ids.size()>0);
    if (ids.size() == 1) {
        return *(ids.begin());
    }
    int gid = TimelineModel::getNextId();
    createGroupItem(gid);

    if (auto ptr = m_parent.lock()) {
        ptr->registerGroup(gid);
    } else {
        qDebug() << "Impossible to create group because the timeline is not available anymore";
        Q_ASSERT(false);
    }
    for (int id : ids) {
        setGroup(getRootId(id), gid);
    }
    return gid;
}

void GroupsModel::ungroupItem(int id)
{
    int gid = getRootId(id);
    if (auto ptr = m_parent.lock()) {
        ptr->deregisterGroup(gid);
    } else {
        qDebug() << "Impossible to ungroup item because the timeline is not available anymore";
        Q_ASSERT(false);
    }

    destructGroupItem(gid);
}

int GroupsModel::getRootId(int id) const
{
    Q_ASSERT(m_upLink.count(id) > 0);
    int father = m_upLink.at(id);
    if (father == -1) {
        return id;
    }
    return getRootId(father);
}

bool GroupsModel::isLeaf(int id) const
{
    Q_ASSERT(m_downLink.count(id) > 0);
    return m_downLink.at(id).size() == 0;
}

std::unordered_set<int> GroupsModel::getSubtree(int id) const
{
    std::unordered_set<int> result;
    result.insert(id);
    std::queue<int> queue;
    queue.push(id);
    while (!queue.empty()) {
        int current = queue.front();
        queue.pop();
        for (const int& child : m_downLink.at(current)) {
            result.insert(child);
            queue.push(child);
        }
    }
    return result;
}

std::unordered_set<int> GroupsModel::getLeaves(int id) const
{
    std::unordered_set<int> result;
    std::queue<int> queue;
    queue.push(id);
    while (!queue.empty()) {
        int current = queue.front();
        queue.pop();
        for (const int& child : m_downLink.at(current)) {
            queue.push(child);
        }
        if (m_downLink.at(current).size() == 0) {
            result.insert(current);
        }
    }
    return result;
}

void GroupsModel::createGroupItem(int id)
{
    Q_ASSERT(m_upLink.count(id) == 0);
    Q_ASSERT(m_downLink.count(id) == 0);
    m_upLink[id] = -1;
    m_downLink[id] = std::unordered_set<int>();
}

void GroupsModel::destructGroupItem(int id)
{
    removeFromGroup(id);
    for (int child : m_downLink[id]) {
        m_upLink[child] = -1;
    }
    m_downLink.erase(id);
    m_upLink.erase(id);
}

void GroupsModel::setGroup(int id, int groupId)
{
    Q_ASSERT(m_upLink.count(id) > 0);
    Q_ASSERT(m_downLink.count(groupId) > 0);
    removeFromGroup(id);
    m_upLink[id] = groupId;
    m_downLink[groupId].insert(id);
}

void GroupsModel::removeFromGroup(int id)
{
    Q_ASSERT(m_upLink.count(id) > 0);
    Q_ASSERT(m_downLink.count(id) > 0);
    int parent = m_upLink[id];
    if (parent != -1) {
        m_downLink[parent].erase(id);
    }
    m_upLink[id] = -1;
}
