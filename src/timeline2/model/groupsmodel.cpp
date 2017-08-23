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
#include "macros.hpp"
#include "timelineitemmodel.hpp"
#include <QDebug>
#include <QModelIndex>
#include <queue>
#include <utility>

GroupsModel::GroupsModel(std::weak_ptr<TimelineItemModel> parent) :
    m_parent(std::move(parent))
    , m_lock(QReadWriteLock::Recursive)
{
}

Fun GroupsModel::groupItems_lambda(int gid, const std::unordered_set<int> &ids, bool temporarySelection, int parent)
{
    QWriteLocker locker(&m_lock);
    return [gid, ids, parent, temporarySelection, this]() {
        createGroupItem(gid);
        if (parent != -1) {
            setGroup(gid, parent);
        }

        Q_ASSERT(m_groupIds.count(gid) == 0);
        m_groupIds.insert(gid);

        auto ptr = m_parent.lock();
        if (ptr) {
            ptr->registerGroup(gid);
        } else {
            qDebug() << "Impossible to create group because the timeline is not available anymore";
            Q_ASSERT(false);
        }
        std::unordered_set<int> roots;
        std::transform(ids.begin(), ids.end(), std::inserter(roots, roots.begin()), [&](int id) { return getRootId(id); });
        for (int id : roots) {
            setGroup(getRootId(id), gid);
            if (!temporarySelection && ptr->isClip(id)) {
                QModelIndex ix = ptr->makeClipIndexFromID(id);
                ptr->dataChanged(ix, ix, {TimelineItemModel::GroupedRole});
            }
        }
        return true;
    };
}

int GroupsModel::groupItems(const std::unordered_set<int> &ids, Fun &undo, Fun &redo, bool temporarySelection, bool force)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(!ids.empty());
    if (ids.size() == 1 && !force) {
        // We do not create a group with only one element. Instead, we return the id of that element
        return *(ids.begin());
    }
    int gid = TimelineModel::getNextId();
    auto operation = groupItems_lambda(gid, ids, temporarySelection);
    if (operation()) {
        auto reverse = destructGroupItem_lambda(gid);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return gid;
    }
    return -1;
}

bool GroupsModel::ungroupItem(int id, Fun &undo, Fun &redo, bool temporarySelection)
{
    QWriteLocker locker(&m_lock);
    int gid = getRootId(id);
    if (m_groupIds.count(gid) == 0) {
        // element is not part of a group
        return false;
    }

    return destructGroupItem(gid, true, undo, redo, temporarySelection);
}

void GroupsModel::createGroupItem(int id)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(id) == 0);
    Q_ASSERT(m_downLink.count(id) == 0);
    m_upLink[id] = -1;
    m_downLink[id] = std::unordered_set<int>();
}

Fun GroupsModel::destructGroupItem_lambda(int id)
{
    QWriteLocker locker(&m_lock);
    return [this, id]() {
        auto ptr = m_parent.lock();
        if (m_groupIds.count(id) > 0) {
            if (ptr) {
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
            if (ptr->isClip(child)) {
                QModelIndex ix = ptr->makeClipIndexFromID(child);
                ptr->dataChanged(ix, ix, {TimelineItemModel::GroupedRole});
            }
        }
        m_downLink.erase(id);
        m_upLink.erase(id);
        return true;
    };
}

bool GroupsModel::destructGroupItem(int id, bool deleteOrphan, Fun &undo, Fun &redo, bool temporarySelection)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(id) > 0);
    int parent = m_upLink[id];
    auto old_children = m_downLink[id];
    auto operation = destructGroupItem_lambda(id);
    if (operation()) {
        auto reverse = groupItems_lambda(id, old_children, temporarySelection, parent);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        if (parent != -1 && m_downLink[parent].empty() && deleteOrphan) {
            return destructGroupItem(parent, true, undo, redo, temporarySelection);
        }
        return true;
    }
    return false;
}

bool GroupsModel::destructGroupItem(int id)
{
    QWriteLocker locker(&m_lock);
    return destructGroupItem_lambda(id)();
}

int GroupsModel::getRootId(int id) const
{
    READ_LOCK();
    std::unordered_set<int> seen; // we store visited ids to detect cycles
    int father = -1;
    do {
        Q_ASSERT(m_upLink.count(id) > 0);
        Q_ASSERT(seen.count(id) == 0);
        seen.insert(id);
        father = m_upLink.at(id);
        if (father != -1) {
            id = father;
        }
    } while (father != -1);
    return id;
}

bool GroupsModel::isLeaf(int id) const
{
    READ_LOCK();
    Q_ASSERT(m_downLink.count(id) > 0);
    return m_downLink.at(id).empty();
}

bool GroupsModel::isInGroup(int id) const
{
    READ_LOCK();
    Q_ASSERT(m_downLink.count(id) > 0);
    return getRootId(id) != id;
}

std::unordered_set<int> GroupsModel::getSubtree(int id) const
{
    READ_LOCK();
    std::unordered_set<int> result;
    result.insert(id);
    std::queue<int> queue;
    queue.push(id);
    while (!queue.empty()) {
        int current = queue.front();
        queue.pop();
        for (const int &child : m_downLink.at(current)) {
            result.insert(child);
            queue.push(child);
        }
    }
    return result;
}

std::unordered_set<int> GroupsModel::getLeaves(int id) const
{
    READ_LOCK();
    std::unordered_set<int> result;
    std::queue<int> queue;
    queue.push(id);
    while (!queue.empty()) {
        int current = queue.front();
        queue.pop();
        for (const int &child : m_downLink.at(current)) {
            queue.push(child);
        }
        if (m_downLink.at(current).empty()) {
            result.insert(current);
        }
    }
    return result;
}

std::unordered_set<int> GroupsModel::getDirectChildren(int id) const
{
    READ_LOCK();
    Q_ASSERT(m_downLink.count(id) > 0);
    return m_downLink.at(id);
}

void GroupsModel::setGroup(int id, int groupId)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(id) > 0);
    Q_ASSERT(groupId == -1 || m_downLink.count(groupId) > 0);
    Q_ASSERT(id != groupId);
    removeFromGroup(id);
    m_upLink[id] = groupId;
    if (groupId != -1)
        m_downLink[groupId].insert(id);
}

void GroupsModel::removeFromGroup(int id)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(id) > 0);
    Q_ASSERT(m_downLink.count(id) > 0);
    int parent = m_upLink[id];
    if (parent != -1) {
        m_downLink[parent].erase(id);
    }
    m_upLink[id] = -1;
}

std::unordered_map<int, int>GroupsModel::groupsData()
{
    return m_upLink;
}

std::unordered_map<int, std::unordered_set<int>>GroupsModel::groupsDataDownlink()
{
    return m_downLink;
}

bool GroupsModel::mergeSingleGroups(int id, Fun &undo, Fun &redo)
{
    // The idea is as follow: we start from the leaves, and go up to the root.
    // In the process, if we find a node with only one children, we flag it for deletion
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(id) > 0);
    auto leaves = getLeaves(id);
    std::unordered_map<int, int> old_parents, new_parents;
    std::vector<int> to_delete;
    std::unordered_set<int> processed; // to avoid going twice along the same branch
    for(int leaf : leaves) {
        int current = m_upLink[leaf];
        int start = leaf;
        while (current != m_upLink[id] && processed.count(current) == 0  ) {
            processed.insert(current);
            if (m_downLink[current].size() == 1) {
                to_delete.push_back(current);
            } else {
                if (current != m_upLink[start]) {
                    old_parents[start] = m_upLink[start];
                    new_parents[start] = current;
                }
                start = current;
            }
            current = m_upLink[current];
        }
        if (current != m_upLink[start]) {
            old_parents[start] = m_upLink[start];
            new_parents[start] = current;
        }
    }
    auto parent_changer = [this](const std::unordered_map<int, int>& parents){
        auto ptr = m_parent.lock();
        if (!ptr) {
            qDebug() << "Impossible to create group because the timeline is not available anymore";
            return false;
        }
        for (const auto& group : parents) {
            int old = m_upLink[group.first];
            setGroup(group.first, group.second);
            if (old == -1 && group.second != -1 && ptr->isClip(group.first)) {
                QModelIndex ix = ptr->makeClipIndexFromID(group.first);
                ptr->dataChanged(ix, ix, {TimelineItemModel::GroupedRole});
            }
        }
        return true;
    };
    Fun reverse = [this, old_parents, parent_changer]() { return parent_changer(old_parents); };
    Fun operation = [this, new_parents, parent_changer]() { return parent_changer(new_parents); };
    bool res = operation();
    if (!res) {
        bool undone = reverse();
        Q_ASSERT(undone);
        return res;
    }
    UPDATE_UNDO_REDO(operation, reverse, undo, redo);

    for (int gid : to_delete) {
        Q_ASSERT(m_downLink[gid].size() == 0);
        res = destructGroupItem(gid, false, undo, redo, false);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return res;
        }
    }
    return true;
}

bool GroupsModel::split(int id, std::function<bool(int)> criterion, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    // This function is valid only for roots (otherwise it is not clear what should be the new parent of the created tree)
    Q_ASSERT(m_upLink[id] == -1);

    // We do a BFS on the tree to copy it
    // We store corresponding nodes
    std::unordered_map<int, int> corresp; //keys are id in the original tree, values are temporary negative id assigned for creation of the new tree
    corresp[-1] = -1;
    // These are the nodes to be moved to new tree
    std::vector<int > to_move;
    // We store the groups (ie the nodes) that are going to be part of the new tree
    // Keys are temporary id (negative) and values are the set of children (true ids in the case of leaves and temporary ids for other nodes)
    std::unordered_map<int, std::unordered_set<int> > new_groups;
    std::queue<int> queue;
    queue.push(id);
    int tempId = -10;
    while (!queue.empty()) {
        int current = queue.front();
        queue.pop();
        if (!isLeaf(current) || criterion(current)) {
            if (isLeaf(current)) {
                to_move.push_back(current);
                new_groups[corresp[m_upLink[current]]].insert(current);
            } else {
                corresp[current] = tempId;
                if (m_upLink[current] != -1)
                    new_groups[corresp[m_upLink[current]]].insert(tempId);
                tempId--;
            }
        }
        for (const int &child : m_downLink.at(current)) {
            queue.push(child);
        }
    }
    // First, we simulate deletion of elements that we have to remove from the original tree
    // A side effect of this is that empty groups will be removed
    for (const auto &leaf : to_move) {
        destructGroupItem(leaf, true, undo, redo, false);
    }

    // we artificially recreate the leaves
    Fun operation = [this, to_move]() {
        for (const auto &leaf : to_move) {
            createGroupItem(leaf);
        }
        return true;
    };
    Fun reverse = [this, to_move]() {
        for (const auto &group : to_move) {
            destructGroupItem(group);
        }
        return true;
    };
    bool res = operation();
    if (!res) {
        return false;
    }
    UPDATE_UNDO_REDO(operation, reverse, undo, redo);

    // We now regroup the items of the new tree to recreate hierarchy.
    // This is equivalent to creating the tree bottom up (starting from the leaves)
    // At each iteration, we create a new node by grouping together elements that are either leaves or already created nodes.
    std::unordered_map<int, int> created_id; // to keep track of node that we create
    while (!new_groups.empty()) {
        int selected = INT_MAX;
        for (const auto &group : new_groups) {
            // we check that all children are already created
            bool ok = true;
            for (int elem : group.second) {
                if (elem < -1 && created_id.count(elem) == 0) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                selected = group.first;
                break;
            }
        }
        Q_ASSERT(selected!=INT_MAX);
        std::unordered_set<int> group;
        for (int elem : new_groups[selected]) {
            group.insert(elem < -1 ? created_id[elem] : elem);
        }
        int gid = groupItems(group, undo, redo, false, true);
        created_id[selected] = gid;
        new_groups.erase(selected);
    }

    mergeSingleGroups(id, undo, redo);
    mergeSingleGroups(created_id[corresp[id]], undo, redo);

    return res;
}


void GroupsModel::setInGroupOf(int id, int targetId)
{
    Q_ASSERT(m_upLink.count(targetId) > 0);
    setGroup(id, m_upLink[targetId]);
}
