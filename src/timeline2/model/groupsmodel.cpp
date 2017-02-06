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

Fun GroupsModel::groupItems_lambda(int gid, const std::unordered_set<int>& ids)
{
    return [gid, ids, this](){
        qDebug() << "grouping items in group"<<gid;
        qDebug() << "Ids";
        for(auto i : ids) qDebug() << i;
        qDebug() << "roots";
        for(auto i : ids) qDebug() << getRootId(i);
        createGroupItem(gid);

        Q_ASSERT(m_groupIds.count(gid) == 0);
        m_groupIds.insert(gid);

        if (auto ptr = m_parent.lock()) {
            ptr->registerGroup(gid);
        } else {
            qDebug() << "Impossible to create group because the timeline is not available anymore";
            Q_ASSERT(false);
        }
        std::unordered_set<int> roots;
        std::transform(ids.begin(), ids.end(), std::inserter(roots, roots.begin()),
                       [&](int id){return getRootId(id);});
        for (int id : roots) {
            setGroup(getRootId(id), gid);
        }
        return true;
    };
}

int GroupsModel::groupItems(const std::unordered_set<int>& ids, Fun &undo, Fun &redo)
{
    Q_ASSERT(ids.size()>0);
    if (ids.size() == 1) {
        // We do not create a group with only one element. Instead, we return the id of that element
        return *(ids.begin());
    }
    int gid = TimelineModel::getNextId();
    auto operation = groupItems_lambda(gid, ids);
    if (operation()) {
        auto reverse = destructGroupItem_lambda(gid, false);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return gid;
    }
    return -1;
}

bool GroupsModel::ungroupItem(int id, Fun& undo, Fun& redo)
{
    int gid = getRootId(id);
    if (m_groupIds.count(gid) == 0) {
        //element is not part of a group
        return false;
    }

    return destructGroupItem(gid, true, undo, redo);
}

void GroupsModel::createGroupItem(int id)
{
    Q_ASSERT(m_upLink.count(id) == 0);
    Q_ASSERT(m_downLink.count(id) == 0);
    m_upLink[id] = -1;
    m_downLink[id] = std::unordered_set<int>();
}

Fun GroupsModel::destructGroupItem_lambda(int id, bool deleteOrphan)
{
    return [this, id, deleteOrphan]() {
        if (m_groupIds.count(id) > 0) {
            if(auto ptr = m_parent.lock()) {
                ptr->deregisterGroup(id);
                m_groupIds.erase(id);
            } else {
                qDebug() << "Impossible to ungroup item because the timeline is not available anymore";
                Q_ASSERT(false);
            }
        }
        removeFromGroup(id);
        for (int child : m_downLink[id]) {
            m_upLink[child] = -1;
        }
        m_downLink.erase(id);
        m_upLink.erase(id);
        return true;
    };
}

bool GroupsModel::destructGroupItem(int id, bool deleteOrphan, Fun &undo, Fun &redo)
{
    Q_ASSERT(m_upLink.count(id) > 0);
    int parent = m_upLink[id];
    auto old_children = m_downLink[id];
    auto operation = destructGroupItem_lambda(id, deleteOrphan);
    if (operation()) {
        auto reverse = groupItems_lambda(id, old_children);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        if (parent != -1 && m_downLink[parent].size() == 0 && deleteOrphan) {
            return destructGroupItem(parent, true, undo, redo);
        }
        return true;
    }
    return false;
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

void GroupsModel::setGroup(int id, int groupId)
{
    Q_ASSERT(m_upLink.count(id) > 0);
    Q_ASSERT(m_downLink.count(groupId) > 0);
    Q_ASSERT(id != groupId);
    qDebug() << "Setting " << id <<"in group"<<groupId;
    removeFromGroup(id);
    m_upLink[id] = groupId;
    m_downLink[groupId].insert(id);
}

void GroupsModel::removeFromGroup(int id)
{
    Q_ASSERT(m_upLink.count(id) > 0);
    Q_ASSERT(m_downLink.count(id) > 0);
    int parent = m_upLink[id];
    qDebug() << "removing " << id <<"from group hierarchy. Is parent is"<<parent;
    if (parent != -1) {
        m_downLink[parent].erase(id);
    }
    m_upLink[id] = -1;
}
