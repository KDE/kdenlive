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

#ifndef GROUPMODEL_H
#define GROUPMODEL_H

#include "definitions.h"
#include "undohelper.hpp"
#include <QReadWriteLock>
#include <memory>
#include <unordered_map>
#include <unordered_set>

class TimelineItemModel;

/* @brief This class represents the group hierarchy. This is basically a tree structure
   In this class, we consider that a groupItem is either a clip or a group
*/

class GroupsModel
{

public:
    GroupsModel() = delete;
    GroupsModel(std::weak_ptr<TimelineItemModel> parent);

    /* @brief Create a group that contains all the given items and returns the id of the created group.
       Note that if an item is already part of a group, its topmost group will be considered instead and added in the newly created group.
       If only one id is provided, no group is created, unless force = true.
       @param ids set containing the items to group.
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
       @param type indicates the type of group we create
       Returns the id of the new group, or -1 on error.
    */
    int groupItems(const std::unordered_set<int> &ids, Fun &undo, Fun &redo, GroupType type = GroupType::Normal, bool force = false);

protected:
    /* Lambda version */
    Fun groupItems_lambda(int gid, const std::unordered_set<int> &ids, GroupType type = GroupType::Normal, int parent = -1);

public:
    /* Deletes the topmost group containing given element
       Note that if the element is not in a group, then it will not be touched.
       Return true on success
       @param id id of the groupitem
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
     */
    bool ungroupItem(int id, Fun &undo, Fun &redo);

    /* @brief Create a groupItem in the hierarchy. Initially it is not part of a group
       @param id id of the groupItem
    */
    void createGroupItem(int id);

    /* @brief Destruct a group item
       Note that this public function expects that the given id is an orphan element.
       @param id id of the groupItem
    */
    bool destructGroupItem(int id);

    /* @brief Merges group with only one child to parent
       Ex:   .                     .
            / \                   / \
           .   .    becomes      a   b
          /     \
         a       b
       @param id id of the tree to consider
     */
    bool mergeSingleGroups(int id, Fun &undo, Fun &redo);

    /* @brief Split the group tree according to a given criterion
       All the leaves satisfying the criterion are moved to the new tree, the other stay
       Both tree are subsequently simplified to avoid weird structure.
       @param id is the root of the tree
     */
    bool split(int id, const std::function<bool(int)> &criterion, Fun &undo, Fun &redo);

    /* @brief Copy a group hierarchy.
       @param mapping describes the correspondence between the ids of the items in the source group hierarchy,
       and their counterpart in the hierarchy that we create.
       It will also be used as a return parameter, by adding the mapping between the groups of the hierarchy
       Note that if the target items should not belong to a group.
    */
    bool copyGroups(std::unordered_map<int, int> &mapping, Fun &undo, Fun &redo);

    /* @brief Get the overall father of a given groupItem
       If the element has no father, it is returned as is.
       @param id id of the groupitem
    */
    int getRootId(int id) const;

    /* @brief Returns true if the groupItem has no descendant
       @param id of the groupItem
    */
    bool isLeaf(int id) const;

    /* @brief Returns true if the element is in a non-trivial group
       @param id of the groupItem
    */
    bool isInGroup(int id) const;

    /* @brief Move element id in the same group as targetId */
    void setInGroupOf(int id, int targetId, Fun &undo, Fun &redo);

    /* @brief We replace the leaf node given by id with a group that contains the leaf plus all the clips in to_add.
     * The created group type is given in parameter
     * Returns true on success
     */
    bool createGroupAtSameLevel(int id, std::unordered_set<int> to_add, GroupType type, Fun &undo, Fun &redo);

    /* @brief Returns the id of all the descendant of given item (including item)
       @param id of the groupItem
    */
    std::unordered_set<int> getSubtree(int id) const;

    /* @brief Returns the id of all the leaves in the subtree of the given item
       This should correspond to the ids of the clips, since they should be the only items with no descendants
       @param id of the groupItem
    */
    std::unordered_set<int> getLeaves(int id) const;

    /* @brief Gets direct children of a given group item
       @param id of the groupItem
     */
    std::unordered_set<int> getDirectChildren(int id) const;

    /* @brief Gets direct ancestor of a given group item. Returns -1 if not in a group
       @param id of the groupItem
    */
    int getDirectAncestor(int id) const;

    /* @brief Get the type of the group
       @param id of the groupItem. Must be a proper group, not a leaf
    */
    GroupType getType(int id) const;

    /* @brief Convert the group hierarchy to json.
       Note that we cannot expect clipId nor groupId to be the same on project reopening, thus we cannot rely on them for saving.
       To workaround that, we currently identify clips by their position + track
    */
    const QString toJson() const;
    const QString toJson(std::unordered_set<int> roots) const;
    bool fromJson(const QString &data);
    bool fromJsonWithOffset(const QString &data, const QMap<int, int> &trackMap, int offset, Fun &undo, Fun &redo);

    /* @brief if the clip belongs to a AVSplit group, then return the id of the other corresponding clip. Otherwise, returns -1 */
    int getSplitPartner(int id) const;

    /* @brief Check the internal consistency of the model. Returns false if something is wrong
       @param failOnSingleGroups: if true, we make sure that a non-leaf node has at least two children
       @param checkTimelineConsistency: if true, we make sure that the group data of the parent timeline are consistent
    */
    bool checkConsistency(bool failOnSingleGroups = true, bool checkTimelineConsistency = false);

protected:
    /* @brief Destruct a groupItem in the hierarchy.
       All its children will become their own roots
       Return true on success
       @param id id of the groupitem
       @param deleteOrphan If this parameter is true, we recursively delete any group that become empty following the destruction
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
    */
    bool destructGroupItem(int id, bool deleteOrphan, Fun &undo, Fun &redo);
    /* Lambda version */
    Fun destructGroupItem_lambda(int id);

    /* @brief change the group of a given item
       @param id of the groupItem
       @param groupId id of the group to assign it to
       @param changeState when false, the grouped role for item won't be updated (for selection)
    */
    void setGroup(int id, int groupId, bool changeState = true);

    /* @brief Remove an item from all the groups it belongs to.
       @param id of the groupItem
    */
    void removeFromGroup(int id);

    /* @brief This is the actual recursive implementation of the copy function. */
    bool processCopy(int gid, std::unordered_map<int, int> &mapping, Fun &undo, Fun &redo);

    /* @brief This is the actual recursive implementation of the conversion to json */
    QJsonObject toJson(int gid) const;

    /* @brief This is the actual recursive implementation of the parsing from json
       Returns the id of the created group
    */
    int fromJson(const QJsonObject &o, Fun &undo, Fun &redo);

    /* @brief Transform a leaf node into a group node of given type. This implies doing the registration to the timeline */
    void promoteToGroup(int gid, GroupType type);

    /* @brief Transform a group node with no children into a leaf. This implies doing the deregistration to the timeline */
    void downgradeToLeaf(int gid);

    /* @Brief helper function to change the type of a group.
       @param id of the groupItem
       @param type: new type of the group
    */
    void setType(int gid, GroupType type);
    
    void adjustOffset(QJsonArray &updatedNodes, QJsonObject childObject, int offset, const QMap<int, int> &trackMap);

private:
    std::weak_ptr<TimelineItemModel> m_parent;

    std::unordered_map<int, int> m_upLink;                       // edges toward parent
    std::unordered_map<int, std::unordered_set<int>> m_downLink; // edges toward children

    std::unordered_map<int, GroupType> m_groupIds; // this keeps track of "real" groups (non-leaf elements), and their types
    mutable QReadWriteLock m_lock;                 // This is a lock that ensures safety in case of concurrent access
};

#endif
