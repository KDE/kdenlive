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
#include "trackmodel.hpp"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QModelIndex>
#include <queue>
#include <stack>
#include <utility>

GroupsModel::GroupsModel(std::weak_ptr<TimelineItemModel> parent)
    : m_parent(std::move(parent))
    , m_lock(QReadWriteLock::Recursive)
{
}

void GroupsModel::promoteToGroup(int gid, GroupType type)
{
    Q_ASSERT(type != GroupType::Leaf);
    Q_ASSERT(m_groupIds.count(gid) == 0);
    m_groupIds.insert({gid, type});

    auto ptr = m_parent.lock();
    if (ptr) {
        // qDebug() << "Registering group" << gid << "of type" << groupTypeToStr(getType(gid));
        ptr->registerGroup(gid);
    } else {
        qDebug() << "Impossible to create group because the timeline is not available anymore";
        Q_ASSERT(false);
    }
}
void GroupsModel::downgradeToLeaf(int gid)
{
    Q_ASSERT(m_groupIds.count(gid) != 0);
    Q_ASSERT(m_downLink.at(gid).size() == 0);
    auto ptr = m_parent.lock();
    if (ptr) {
        // qDebug() << "Deregistering group" << gid << "of type" << groupTypeToStr(getType(gid));
        ptr->deregisterGroup(gid);
        m_groupIds.erase(gid);
    } else {
        qDebug() << "Impossible to ungroup item because the timeline is not available anymore";
        Q_ASSERT(false);
    }
}

Fun GroupsModel::groupItems_lambda(int gid, const std::unordered_set<int> &ids, GroupType type, int parent)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(ids.size() == 0 || type != GroupType::Leaf);
    return [gid, ids, parent, type, this]() {
        createGroupItem(gid);
        if (parent != -1) {
            setGroup(gid, parent);
        }

        if (ids.size() > 0) {
            promoteToGroup(gid, type);
            std::unordered_set<int> roots;
            std::transform(ids.begin(), ids.end(), std::inserter(roots, roots.begin()), [&](int id) { return getRootId(id); });
            auto ptr = m_parent.lock();
            if (!ptr) Q_ASSERT(false);
            for (int id : roots) {
                if (type != GroupType::Selection) {
                    setGroup(getRootId(id), gid, true);
                } else {
                    setGroup(getRootId(id), gid, false);
                    ptr->setSelected(id, true);
                }
            }
        }
        return true;
    };
}

int GroupsModel::groupItems(const std::unordered_set<int> &ids, Fun &undo, Fun &redo, GroupType type, bool force)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(type != GroupType::Leaf);
    Q_ASSERT(!ids.empty());
    std::unordered_set<int> roots;
    std::transform(ids.begin(), ids.end(), std::inserter(roots, roots.begin()), [&](int id) { return getRootId(id); });
    if (roots.size() == 1 && !force) {
        // We do not create a group with only one element. Instead, we return the id of that element
        return *(roots.begin());
    }

    int gid = TimelineModel::getNextId();
    auto operation = groupItems_lambda(gid, roots, type);
    if (operation()) {
        auto reverse = destructGroupItem_lambda(gid);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return gid;
    }
    return -1;
}

bool GroupsModel::ungroupItem(int id, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    int gid = getRootId(id);
    if (m_groupIds.count(gid) == 0) {
        // element is not part of a group
        return false;
    }

    return destructGroupItem(gid, true, undo, redo);
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
        removeFromGroup(id);
        auto ptr = m_parent.lock();
        if (!ptr) Q_ASSERT(false);
        for (int child : m_downLink[id]) {
            m_upLink[child] = -1;
            QModelIndex ix;
            if (ptr->isClip(child)) {
                ix = ptr->makeClipIndexFromID(child);
            } else if (ptr->isComposition(child)) {
                ix = ptr->makeCompositionIndexFromID(child);
            }
            if (ix.isValid()) {
                emit ptr->dataChanged(ix, ix, {TimelineModel::GroupedRole});
            }
        }
        m_downLink[id].clear();
        if (getType(id) != GroupType::Leaf) {
            downgradeToLeaf(id);
        }
        m_downLink.erase(id);
        m_upLink.erase(id);
        return true;
    };
}

bool GroupsModel::destructGroupItem(int id, bool deleteOrphan, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(id) > 0);
    int parent = m_upLink[id];
    auto old_children = m_downLink[id];
    auto old_type = getType(id);
    auto old_parent_type = GroupType::Normal;
    if (parent != -1) {
        old_parent_type = getType(parent);
    }
    auto operation = destructGroupItem_lambda(id);
    if (operation()) {
        auto reverse = groupItems_lambda(id, old_children, old_type, parent);
        // we may need to reset the group of the parent
        if (parent != -1) {
            auto setParent = [&, old_parent_type, parent]() {
                setType(parent, old_parent_type);
                return true;
            };
            PUSH_LAMBDA(setParent, reverse);
        }
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        if (parent != -1 && m_downLink[parent].empty() && deleteOrphan) {
            return destructGroupItem(parent, true, undo, redo);
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

int GroupsModel::getSplitPartner(int id) const
{
    READ_LOCK();
    Q_ASSERT(m_downLink.count(id) > 0);
    int groupId = m_upLink.at(id);
    if (groupId == -1 || getType(groupId) != GroupType::AVSplit) {
        // clip does not have an AV split partner
        return -1;
    }
    std::unordered_set<int> leaves = getDirectChildren(groupId);
    if (leaves.size() != 2) {
        // clip does not have an AV split partner
        qDebug() << "WRONG SPLIT GROUP SIZE: " << leaves.size();
        return -1;
    }
    for (const int &child : leaves) {
        if (child != id) {
            return child;
        }
    }
    return -1;
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
int GroupsModel::getDirectAncestor(int id) const
{
    READ_LOCK();
    Q_ASSERT(m_upLink.count(id) > 0);
    return m_upLink.at(id);
}

void GroupsModel::setGroup(int id, int groupId, bool changeState)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(id) > 0);
    Q_ASSERT(groupId == -1 || m_downLink.count(groupId) > 0);
    Q_ASSERT(id != groupId);
    removeFromGroup(id);
    m_upLink[id] = groupId;
    if (groupId != -1) {
        m_downLink[groupId].insert(id);
        auto ptr = m_parent.lock();
        if (changeState && ptr) {
            QModelIndex ix;
            if (ptr->isClip(id)) {
                ix = ptr->makeClipIndexFromID(id);
            } else if (ptr->isComposition(id)) {
                ix = ptr->makeCompositionIndexFromID(id);
            }
            if (ix.isValid()) {
                emit ptr->dataChanged(ix, ix, {TimelineModel::GroupedRole});
            }
        }
        if (getType(groupId) == GroupType::Leaf) {
            promoteToGroup(groupId, GroupType::Normal);
        }
    }
}

void GroupsModel::removeFromGroup(int id)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(id) > 0);
    Q_ASSERT(m_downLink.count(id) > 0);
    int parent = m_upLink[id];
    if (parent != -1) {
        Q_ASSERT(getType(parent) != GroupType::Leaf);
        m_downLink[parent].erase(id);
        QModelIndex ix;
        auto ptr = m_parent.lock();
        if (!ptr) Q_ASSERT(false);
        if (ptr->isClip(id)) {
            ix = ptr->makeClipIndexFromID(id);
        } else if (ptr->isComposition(id)) {
            ix = ptr->makeCompositionIndexFromID(id);
        }
        if (ix.isValid()) {
            emit ptr->dataChanged(ix, ix, {TimelineModel::GroupedRole});
        }
        if (m_downLink[parent].size() == 0) {
            downgradeToLeaf(parent);
        }
    }
    m_upLink[id] = -1;
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
    for (int leaf : leaves) {
        int current = m_upLink[leaf];
        int start = leaf;
        while (current != m_upLink[id] && processed.count(current) == 0) {
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
    auto parent_changer = [this](const std::unordered_map<int, int> &parents) {
        auto ptr = m_parent.lock();
        if (!ptr) {
            qDebug() << "Impossible to create group because the timeline is not available anymore";
            return false;
        }
        for (const auto &group : parents) {
            setGroup(group.first, group.second);
        }
        return true;
    };
    Fun reverse = [old_parents, parent_changer]() { return parent_changer(old_parents); };
    Fun operation = [new_parents, parent_changer]() { return parent_changer(new_parents); };
    bool res = operation();
    if (!res) {
        bool undone = reverse();
        Q_ASSERT(undone);
        return res;
    }
    UPDATE_UNDO_REDO(operation, reverse, undo, redo);

    for (int gid : to_delete) {
        Q_ASSERT(m_downLink[gid].size() == 0);
        if (getType(gid) == GroupType::Selection) {
            continue;
        }
        res = destructGroupItem(gid, false, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return res;
        }
    }
    return true;
}

bool GroupsModel::split(int id, const std::function<bool(int)> &criterion, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (isLeaf(id)) {
        return true;
    }
    // This function is valid only for roots (otherwise it is not clear what should be the new parent of the created tree)
    Q_ASSERT(m_upLink[id] == -1);
    Q_ASSERT(m_groupIds[id] != GroupType::Selection);
    bool regroup = true; // we don't support splitting if selection group is active
    // We do a BFS on the tree to copy it
    // We store corresponding nodes
    std::unordered_map<int, int> corresp; // keys are id in the original tree, values are temporary negative id assigned for creation of the new tree
    corresp[-1] = -1;
    // These are the nodes to be moved to new tree
    std::vector<int> to_move;
    // We store the groups (ie the nodes) that are going to be part of the new tree
    // Keys are temporary id (negative) and values are the set of children (true ids in the case of leaves and temporary ids for other nodes)
    std::unordered_map<int, std::unordered_set<int>> new_groups;
    // We store also the target type of the new groups
    std::unordered_map<int, GroupType> new_types;
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
                new_types[tempId] = getType(current);
                if (m_upLink[current] != -1) new_groups[corresp[m_upLink[current]]].insert(tempId);
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
        destructGroupItem(leaf, true, undo, redo);
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

    // We prune the new_groups to remove empty ones
    bool finished = false;
    while (!finished) {
        finished = true;
        int selected = INT_MAX;
        for (const auto &it : new_groups) {
            if (it.second.size() == 0) { // empty group
                finished = false;
                selected = it.first;
                break;
            }
            for (int it2 : it.second) {
                if (it2 < -1 && new_groups.count(it2) == 0) {
                    // group that has no reference, it is empty too
                    finished = false;
                    selected = it2;
                    break;
                }
            }
            if (!finished) break;
        }
        if (!finished) {
            new_groups.erase(selected);
            for (auto &new_group : new_groups) {
                new_group.second.erase(selected);
            }
        }
    }
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
        Q_ASSERT(selected != INT_MAX);
        std::unordered_set<int> group;
        for (int elem : new_groups[selected]) {
            group.insert(elem < -1 ? created_id[elem] : elem);
        }
        Q_ASSERT(new_types.count(selected) != 0);
        int gid = groupItems(group, undo, redo, new_types[selected], true);
        created_id[selected] = gid;
        new_groups.erase(selected);
    }

    if (regroup) {
        if (m_groupIds.count(id) > 0) {
            mergeSingleGroups(id, undo, redo);
        }
        if (created_id[corresp[id]]) {
            mergeSingleGroups(created_id[corresp[id]], undo, redo);
        }
    }

    return res;
}

void GroupsModel::setInGroupOf(int id, int targetId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(targetId) > 0);
    Fun operation = [this, id, group = m_upLink[targetId]]() {
        setGroup(id, group);
        return true;
    };
    Fun reverse = [this, id, group = m_upLink[id]]() {
        setGroup(id, group);
        return true;
    };
    operation();
    UPDATE_UNDO_REDO(operation, reverse, undo, redo);
}

bool GroupsModel::createGroupAtSameLevel(int id, std::unordered_set<int> to_add, GroupType type, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(id) > 0);
    Q_ASSERT(isLeaf(id));
    if (to_add.size() == 0) {
        return true;
    }
    int gid = TimelineModel::getNextId();
    std::unordered_map<int, int> old_parents;
    to_add.insert(id);

    for (int g : to_add) {
        Q_ASSERT(m_upLink.count(g) > 0);
        old_parents[g] = m_upLink[g];
    }
    Fun operation = [this, gid, type, to_add, parent = m_upLink.at(id)]() {
        createGroupItem(gid);
        setGroup(gid, parent);
        for (const auto &g : to_add) {
            setGroup(g, gid);
        }
        setType(gid, type);
        return true;
    };
    Fun reverse = [this, old_parents, gid]() {
        for (const auto &g : old_parents) {
            setGroup(g.first, g.second);
        }
        setGroup(gid, -1);
        destructGroupItem_lambda(gid)();
        return true;
    };
    bool success = operation();
    if (success) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    }
    return success;
}

bool GroupsModel::processCopy(int gid, std::unordered_map<int, int> &mapping, Fun &undo, Fun &redo)
{
    qDebug() << "processCopy" << gid;
    if (isLeaf(gid)) {
        qDebug() << "it is a leaf";
        return true;
    }
    bool ok = true;
    std::unordered_set<int> targetGroup;
    for (int child : m_downLink.at(gid)) {
        ok = ok && processCopy(child, mapping, undo, redo);
        if (!ok) {
            break;
        }
        targetGroup.insert(mapping.at(child));
    }
    qDebug() << "processCopy" << gid << "success of child" << ok;
    if (ok && m_groupIds[gid] != GroupType::Selection) {
        int id = groupItems(targetGroup, undo, redo);
        qDebug() << "processCopy" << gid << "created id" << id;
        if (id != -1) {
            mapping[gid] = id;
            return true;
        }
    }
    return ok;
}

bool GroupsModel::copyGroups(std::unordered_map<int, int> &mapping, Fun &undo, Fun &redo)
{
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    // destruct old groups for the targets items
    for (const auto &corresp : mapping) {
        ungroupItem(corresp.second, local_undo, local_redo);
    }
    std::unordered_set<int> roots;
    std::transform(mapping.begin(), mapping.end(), std::inserter(roots, roots.begin()),
                   [&](decltype(*mapping.begin()) corresp) { return getRootId(corresp.first); });
    bool res = true;
    qDebug() << "found" << roots.size() << "roots";
    for (int r : roots) {
        qDebug() << "processing copy for root " << r;
        res = res && processCopy(r, mapping, local_undo, local_redo);
        if (!res) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

GroupType GroupsModel::getType(int id) const
{
    if (m_groupIds.count(id) > 0) {
        return m_groupIds.at(id);
    }
    return GroupType::Leaf;
}

QJsonObject GroupsModel::toJson(int gid) const
{
    QJsonObject currentGroup;
    currentGroup.insert(QLatin1String("type"), QJsonValue(groupTypeToStr(getType(gid))));
    if (m_groupIds.count(gid) > 0) {
        // in that case, we have a proper group
        QJsonArray array;
        Q_ASSERT(m_downLink.count(gid) > 0);
        for (int c : m_downLink.at(gid)) {
            array.push_back(toJson(c));
        }
        currentGroup.insert(QLatin1String("children"), array);
    } else {
        // in that case we have a clip or composition
        if (auto ptr = m_parent.lock()) {
            Q_ASSERT(ptr->isClip(gid) || ptr->isComposition(gid));
            currentGroup.insert(QLatin1String("leaf"), QJsonValue(QLatin1String(ptr->isClip(gid) ? "clip" : "composition")));
            int track = ptr->getTrackPosition(ptr->getItemTrackId(gid));
            int pos = ptr->getItemPosition(gid);
            currentGroup.insert(QLatin1String("data"), QJsonValue(QString("%1:%2").arg(track).arg(pos)));
        } else {
            qDebug() << "Impossible to create group because the timeline is not available anymore";
            Q_ASSERT(false);
        }
    }
    return currentGroup;
}

const QString GroupsModel::toJson() const
{
    std::unordered_set<int> roots;
    std::transform(m_groupIds.begin(), m_groupIds.end(), std::inserter(roots, roots.begin()),
                   [&](decltype(*m_groupIds.begin()) g) { 
        const int parentId = getRootId(g.first);
        if (getType(parentId) == GroupType::Selection) {
            // Don't insert selection group, only its child groups
            return g.first;
        }
        return parentId;
    });
    QJsonArray list;
    for (int r : roots) {
        list.push_back(toJson(r));
    }
    QJsonDocument json(list);
    return QString(json.toJson());
}

const QString GroupsModel::toJson(std::unordered_set<int> roots) const
{
    QJsonArray list;
    for (int r : roots) {
        if (getType(r) != GroupType::Selection) list.push_back(toJson(r));
    }
    QJsonDocument json(list);
    return QString(json.toJson());
}

int GroupsModel::fromJson(const QJsonObject &o, Fun &undo, Fun &redo)
{
    if (!o.contains(QLatin1String("type"))) {
        qDebug() << "CANNOT PARSE GROUP DATA";
        return -1;
    }
    auto type = groupTypeFromStr(o.value(QLatin1String("type")).toString());
    if (type == GroupType::Leaf) {
        if (auto ptr = m_parent.lock()) {
            if (!o.contains(QLatin1String("data")) || !o.contains(QLatin1String("leaf"))) {
                qDebug() << "Error: missing info in the group structure while parsing json";
                return -1;
            }
            QString data = o.value(QLatin1String("data")).toString();
            QString leaf = o.value(QLatin1String("leaf")).toString();
            int trackId = ptr->getTrackIndexFromPosition(data.section(":", 0, 0).toInt());
            int pos = data.section(":", 1, 1).toInt();
            int id = -1;
            if (leaf == QLatin1String("clip")) {
                id = ptr->getClipByPosition(trackId, pos);
            } else if (leaf == QLatin1String("composition")) {
                id = ptr->getCompositionByPosition(trackId, pos);
            } else {
                qDebug() << " * * *UNKNOWN ITEM: " << leaf;
            }
            return id;
        } else {
            qDebug() << "Impossible to create group because the timeline is not available anymore";
            Q_ASSERT(false);
        }
    } else {
        if (!o.contains(QLatin1String("children"))) {
            qDebug() << "Error: missing info in the group structure while parsing json";
            return -1;
        }
        auto value = o.value(QLatin1String("children"));
        if (!value.isArray()) {
            qDebug() << "Error : Expected json array of children while parsing groups";
            return -1;
        }
        const auto children = value.toArray();
        std::unordered_set<int> ids;
        for (const auto &c : children) {
            if (!c.isObject()) {
                qDebug() << "Error : Expected json object while parsing groups";
                return -1;
            }
            ids.insert(fromJson(c.toObject(), undo, redo));
        }
        if (ids.count(-1) > 0 || type == GroupType::Selection) {
            return -1;
        }
        return groupItems(ids, undo, redo, type);
    }
    return -1;
}

bool GroupsModel::fromJson(const QString &data)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    auto json = QJsonDocument::fromJson(data.toUtf8());
    if (!json.isArray()) {
        qDebug() << "Error : Json file should be an array";
        return false;
    }
    const auto list = json.array();
    bool ok = true;
    for (const auto &elem : list) {
        if (!elem.isObject()) {
            qDebug() << "Error : Expected json object while parsing groups";
            undo();
            return false;
        }
        ok = ok && fromJson(elem.toObject(), undo, redo);
    }
    return ok;
}

void GroupsModel::adjustOffset(QJsonArray &updatedNodes, QJsonObject childObject, int offset, const QMap<int, int> &trackMap)
{
    auto value = childObject.value(QLatin1String("children"));
    auto children = value.toArray();
    for (const auto &c : qAsConst(children)) {
        if (!c.isObject()) {
            continue;
        }
        auto child = c.toObject();
        auto type = groupTypeFromStr(child.value(QLatin1String("type")).toString());
        if (child.contains(QLatin1String("data"))) {
            if (auto ptr = m_parent.lock()) {
                QString cur_data = child.value(QLatin1String("data")).toString();
                int trackId = cur_data.section(":", 0, 0).toInt();
                int pos = cur_data.section(":", 1, 1).toInt();
                int trackPos = ptr->getTrackPosition(trackMap.value(trackId));
                pos += offset;
                child.insert(QLatin1String("data"), QJsonValue(QString("%1:%2").arg(trackPos).arg(pos)));
                updatedNodes.append(QJsonValue(child));
            }
        } else if (type != GroupType::Leaf) {
            QJsonObject currentGroup;
            currentGroup.insert(QLatin1String("type"), QJsonValue(groupTypeToStr(type)));
            QJsonArray array;
            adjustOffset(array, child, offset, trackMap);
            currentGroup.insert(QLatin1String("children"), array);
            updatedNodes.append(QJsonValue(currentGroup));
        }
    }
}

bool GroupsModel::fromJsonWithOffset(const QString &data, const QMap<int, int> &trackMap, int offset, Fun &undo, Fun &redo)
{
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    auto json = QJsonDocument::fromJson(data.toUtf8());
    if (!json.isArray()) {
        qDebug() << "Error : Json file should be an array";
        return false;
    }
    auto list = json.array();
    QJsonArray newGroups;
    bool ok = true;
    for (const auto &elem : qAsConst(list)) {
        if (!elem.isObject()) {
            qDebug() << "Error : Expected json object while parsing groups";
            local_undo();
            return false;
        }
        QJsonObject obj = elem.toObject();
        QJsonArray updatedNodes;
        auto type = groupTypeFromStr(obj.value(QLatin1String("type")).toString());
        auto value = obj.value(QLatin1String("children"));
        if (!value.isArray()) {
            qDebug() << "Error : Expected json array of children while parsing groups";
            continue;
        }
        // Adjust offset
        adjustOffset(updatedNodes, obj, offset, trackMap);
        QJsonObject currentGroup;
        currentGroup.insert(QLatin1String("children"), QJsonValue(updatedNodes));
        currentGroup.insert(QLatin1String("type"), QJsonValue(groupTypeToStr(type)));
        newGroups.append(QJsonValue(currentGroup));
    }

    // Group
    for (const auto &elem : newGroups) {
        if (!elem.isObject()) {
            qDebug() << "Error : Expected json object while parsing groups";
            break;
        }
        ok = ok && fromJson(elem.toObject(), local_undo, local_redo);
    }
    if (ok) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    } else {
        bool undone = local_undo();
        Q_ASSERT(undone);
    }
    return ok;
}

void GroupsModel::setType(int gid, GroupType type)
{
    Q_ASSERT(m_groupIds.count(gid) != 0);
    if (type == GroupType::Leaf) {
        Q_ASSERT(m_downLink[gid].size() == 0);
        if (m_groupIds.count(gid) > 0) {
            m_groupIds.erase(gid);
        }
    } else {
        m_groupIds[gid] = type;
    }
}

bool GroupsModel::checkConsistency(bool failOnSingleGroups, bool checkTimelineConsistency)
{
    // check that all element with up link have a down link
    for (const auto &elem : m_upLink) {
        if (m_downLink.count(elem.first) == 0) {
            qDebug() << "ERROR: Group model has missing up/down links";
            return false;
        }
    }
    // check that all element with down link have a up link
    for (const auto &elem : m_downLink) {
        if (m_upLink.count(elem.first) == 0) {
            qDebug() << "ERROR: Group model has missing up/down links";
            return false;
        }
    }

    int selectionCount = 0;
    for (const auto &elem : m_upLink) {
        // iterate through children to check links
        for (const auto &child : m_downLink[elem.first]) {
            if (m_upLink[child] != elem.first) {
                qDebug() << "ERROR: Group model has inconsistent up/down links";
                return false;
            }
        }
        bool isLeaf = m_downLink[elem.first].empty();
        if (isLeaf) {
            if (m_groupIds.count(elem.first) > 0) {
                qDebug() << "ERROR: Group model has wrong tracking of non-leaf groups";
                return false;
            }
        } else {
            if (m_groupIds.count(elem.first) == 0) {
                qDebug() << "ERROR: Group model has wrong tracking of non-leaf groups";
                return false;
            }
            if (m_downLink[elem.first].size() == 1 && failOnSingleGroups) {
                qDebug() << "ERROR: Group model contains groups with single element";
                return false;
            }
            if (getType(elem.first) == GroupType::Selection) {
                selectionCount++;
            }
            if (elem.second != -1 && getType(elem.first) == GroupType::Selection) {
                qDebug() << "ERROR: Group model contains inner groups of selection type";
                return false;
            }
            if (getType(elem.first) == GroupType::Leaf) {
                qDebug() << "ERROR: Group model contains groups of Leaf type";
                return false;
            }
        }
    }
    if (selectionCount > 1) {
        qDebug() << "ERROR: Found too many selections: " << selectionCount;
        return false;
    }

    // Finally, we do a depth first visit of the tree to check for loops
    std::unordered_set<int> visited;
    for (const auto &elem : m_upLink) {
        if (elem.second == -1) {
            // this is a root, traverse the tree from here
            std::stack<int> stack;
            stack.push(elem.first);
            while (!stack.empty()) {
                int cur = stack.top();
                stack.pop();
                if (visited.count(cur) > 0) {
                    qDebug() << "ERROR: Group model contains a cycle";
                    return false;
                }
                visited.insert(cur);
                for (int child : m_downLink[cur]) {
                    stack.push(child);
                }
            }
        }
    }

    // Do a last pass to check everybody was visited
    for (const auto &elem : m_upLink) {
        if (visited.count(elem.first) == 0) {
            qDebug() << "ERROR: Group model contains unreachable elements";
            return false;
        }
    }

    if (checkTimelineConsistency) {
        if (auto ptr = m_parent.lock()) {
            auto isTimelineObject = [&](int cid) { return ptr->isClip(cid) || ptr->isComposition(cid); };
            for (int g : ptr->m_allGroups) {
                if (m_upLink.count(g) == 0 || getType(g) == GroupType::Leaf) {
                    qDebug() << "ERROR: Timeline contains inconsistent group data";
                    return false;
                }
            }
            for (const auto &elem : m_upLink) {
                if (getType(elem.first) == GroupType::Leaf) {
                    if (!isTimelineObject(elem.first)) {
                        qDebug() << "ERROR: Group model contains leaf element that is not a clip nor a composition";
                        return false;
                    }
                } else {
                    if (ptr->m_allGroups.count(elem.first) == 0) {
                        qDebug() << "ERROR: Group model contains group element that is not  registered on timeline";
                        Q_ASSERT(false);
                        return false;
                    }
                    if (getType(elem.first) == GroupType::AVSplit) {
                        if (m_downLink[elem.first].size() != 2) {
                            qDebug() << "ERROR: Group model contains a AVSplit group with a children count != 2";
                            return false;
                        }
                        auto it = m_downLink[elem.first].begin();
                        int cid1 = (*it);
                        ++it;
                        int cid2 = (*it);
                        if (!isTimelineObject(cid1) || !isTimelineObject(cid2)) {
                            qDebug() << "ERROR: Group model contains an AVSplit group with invalid members";
                            return false;
                        }
                        int tid1 = ptr->getClipTrackId(cid1);
                        bool isAudio1 = ptr->getTrackById(tid1)->isAudioTrack();
                        int tid2 = ptr->getClipTrackId(cid2);
                        bool isAudio2 = ptr->getTrackById(tid2)->isAudioTrack();
                        if (isAudio1 == isAudio2) {
                            qDebug() << "ERROR: Group model contains an AVSplit formed with members that are both on an audio track or on a video track";
                            return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}
