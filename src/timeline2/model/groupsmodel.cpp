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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QModelIndex>
#include <queue>
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
                setGroup(getRootId(id), gid);
                if (type != GroupType::Selection && ptr->isClip(id)) {
                    QModelIndex ix = ptr->makeClipIndexFromID(id);
                    ptr->dataChanged(ix, ix, {TimelineModel::GroupedRole});
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
    if (ids.size() == 1 && !force) {
        // We do not create a group with only one element. Instead, we return the id of that element
        return *(ids.begin());
    }
    int gid = TimelineModel::getNextId();
    auto operation = groupItems_lambda(gid, ids, type);
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
            if (ptr->isClip(child)) {
                QModelIndex ix = ptr->makeClipIndexFromID(child);
                ptr->dataChanged(ix, ix, {TimelineModel::GroupedRole});
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

void GroupsModel::setGroup(int id, int groupId)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_upLink.count(id) > 0);
    Q_ASSERT(groupId == -1 || m_downLink.count(groupId) > 0);
    Q_ASSERT(id != groupId);
    removeFromGroup(id);
    m_upLink[id] = groupId;
    if (groupId != -1) {
        m_downLink[groupId].insert(id);
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
            int old = m_upLink[group.first];
            setGroup(group.first, group.second);
            if (old == -1 && group.second != -1 && ptr->isClip(group.first)) {
                QModelIndex ix = ptr->makeClipIndexFromID(group.first);
                ptr->dataChanged(ix, ix, {TimelineModel::GroupedRole});
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
    bool regroup = m_groupIds[id] != GroupType::Selection;
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
                if (m_groupIds[getRootId(current)] != GroupType::Selection) {
                    to_move.push_back(current);
                    new_groups[corresp[m_upLink[current]]].insert(current);
                }
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
            for (auto it = new_groups.begin(); it != new_groups.end(); ++it) {
                (*it).second.erase(selected);
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
        mergeSingleGroups(id, undo, redo);
        mergeSingleGroups(created_id[corresp[id]], undo, redo);
    }

    return res;
}

void GroupsModel::setInGroupOf(int id, int targetId, Fun &undo, Fun &redo)
{
    Q_ASSERT(m_upLink.count(targetId) > 0);
    Fun operation = [ this, id, group = m_upLink[targetId] ]()
    {
        setGroup(id, group);
        return true;
    };
    Fun reverse = [ this, id, group = m_upLink[id] ]()
    {
        setGroup(id, group);
        return true;
    };
    operation();
    UPDATE_UNDO_REDO(operation, reverse, undo, redo);
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
                   [&](decltype(*m_groupIds.begin()) g) { return getRootId(g.first); });
    QJsonArray list;
    for (int r : roots) {
        list.push_back(toJson(r));
    }
    QJsonDocument json(list);
    return QString(json.toJson());
}

int GroupsModel::fromJson(const QJsonObject &o, Fun &undo, Fun &redo)
{
    if (!o.contains(QLatin1String("type"))) {
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
        if (ids.count(-1) > 0) {
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
