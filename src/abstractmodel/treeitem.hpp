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

#ifndef TREEITEM_H
#define TREEITEM_H

#include "definitions.h"
#include <QList>
#include <QVariant>
#include <memory>
#include <unordered_map>

/* @brief This class is a generic class to represent items of a tree-like model
   It works in tandem with AbstractTreeModel or one of its derived classes.
   There is a registration mechanism that takes place: each TreeItem holds a unique Id
   that can allow to retrieve it directly from the model.

   A TreeItem registers itself to the model as soon as it gets a proper parent (the node
   above it in the hierarchy). This means that upon creation, the TreeItem is NOT
   registered, because at this point it doesn't belong to any parent.
   The only exception is for the rootItem, which is always registered.

   Note that the root is a special object. In particular, it must stay at the root and
   must not be declared as the child of any other item.
 */

class AbstractTreeModel;
class TreeItem : public enable_shared_from_this_virtual<TreeItem>
{
public:
    /* @brief Construct a TreeItem
     @param data List of data elements (columns) of the created item
     @param model Pointer to the model to which this elem belongs to
     @param parentItem address of the parent if the child is not orphan
     @param isRoot is true if the object is the topmost item of the tree
     @param id of the newly created item. If left to -1, the id is assigned automatically
     @return a ptr to the constructed item
    */
    static std::shared_ptr<TreeItem> construct(const QList<QVariant> &data, std::shared_ptr<AbstractTreeModel> model, bool isRoot, int id = -1);

    friend class AbstractTreeModel;

protected:
    // This is protected. Call construct instead
    explicit TreeItem(QList<QVariant> data, const std::shared_ptr<AbstractTreeModel> &model, bool isRoot, int id = -1);

public:
    virtual ~TreeItem();

    /* @brief Creates a child of the current item
       @param data: List of data elements (columns) to init the child with.
    */
    std::shared_ptr<TreeItem> appendChild(const QList<QVariant> &data);

    /* @brief Appends an already created child
       Useful for example if the child should be a subclass of TreeItem
       @return true on success. Otherwise, nothing is modified.
    */
    bool appendChild(const std::shared_ptr<TreeItem> &child);
    void moveChild(int ix, const std::shared_ptr<TreeItem> &child);

    /* @brief Remove given child from children list. The parent of the child is updated
       accordingly
     */
    void removeChild(const std::shared_ptr<TreeItem> &child);

    /* @brief Change the parent of the current item. Structures are modified accordingly
     */
    virtual bool changeParent(std::shared_ptr<TreeItem> newParent);

    /* @brief Retrieves a child of the current item
       @param row is the index of the child to retrieve
    */
    std::shared_ptr<TreeItem> child(int row) const;

    /* @brief Returns a vector containing a pointer to all the leaves in the subtree rooted in this element */
    std::vector<std::shared_ptr<TreeItem>> getLeaves();

    /* @brief Return the number of children */
    int childCount() const;

    /* @brief Return the number of data fields (columns) */
    int columnCount() const;

    /* @brief Return the content of a column
       @param column Index of the column to look-up
    */
    QVariant dataColumn(int column) const;
    void setData(int column, const QVariant &dataColumn);

    /* @brief Return the index of current item amongst father's children
       Returns -1 on error (eg: no parent set)
     */
    int row() const;

    /* @brief Return a ptr to the parent item
     */
    std::weak_ptr<TreeItem> parentItem() const;

    /* @brief Return the depth of the current item*/
    int depth() const;

    /* @brief Return the id of the current item*/
    int getId() const;

    /* @brief Return true if the current item has been registered */
    bool isInModel() const;

    /* @brief This is similar to the std::accumulate function, except that it
       operates on the whole subtree
       @param init is the initial value of the operation
       @param is the binary op to apply (signature should be (T, shared_ptr<TreeItem>)->T)
    */
    template <class T, class BinaryOperation> T accumulate(T init, BinaryOperation op);
    template <class T, class BinaryOperation> T accumulate_const(T init, BinaryOperation op) const;

    /* @brief Return true if the current item has the item with given id as an ancestor */
    bool hasAncestor(int id);

    /* @brief Return true if the item thinks it is a root.
       Note that it should be consistent with what the model thinks, but it may have been
       messed up at some point if someone wrongly constructed the object with isRoot = true */
    bool isRoot() const;

protected:
    /* @brief Finish construction of object given its pointer
       This is a separated function so that it can be called from derived classes */
    static void baseFinishConstruct(const std::shared_ptr<TreeItem> &self);

    /* @brief Helper functions to handle registration / deregistration to the model */
    static void registerSelf(const std::shared_ptr<TreeItem> &self);
    void deregisterSelf();

    /* @brief Reflect update of the parent ptr (for example set the correct depth)
       This is meant to be overridden in derived classes
       @param ptr is the pointer to the new parent
    */
    virtual void updateParent(std::shared_ptr<TreeItem> parent);

    std::list<std::shared_ptr<TreeItem>> m_childItems;
    std::unordered_map<int, std::list<std::shared_ptr<TreeItem>>::iterator>
        m_iteratorTable; // this logs the iterator associated which each child id. This allows easy access of a child based on its id.

    QList<QVariant> m_itemData;
    std::weak_ptr<TreeItem> m_parentItem;

    std::weak_ptr<AbstractTreeModel> m_model;
    int m_depth;
    int m_id;

    bool m_isInModel;
    bool m_isRoot;
};

template <class T, class BinaryOperation> T TreeItem::accumulate(T init, BinaryOperation op)
{
    T res = op(init, shared_from_this());
    for (const auto &c : m_childItems) {
        res = c->accumulate(res, op);
    }
    return res;
}
template <class T, class BinaryOperation> T TreeItem::accumulate_const(T init, BinaryOperation op) const
{
    T res = op(init, shared_from_this());
    for (const auto &c : m_childItems) {
        res = c->accumulate_const(res, op);
    }
    return res;
}
#endif
