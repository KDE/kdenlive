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

#include <QList>
#include <QVariant>
#include <memory>
#include <unordered_map>

/* @brief This class is a generic class to represent items of a tree-like model
 */

class AbstractTreeModel;
class TreeItem : public QObject, public std::enable_shared_from_this<TreeItem>
{
public:
    /* @brief Construct a TreeItem
     @param data List of data elements (columns) of the created item
     @param model Pointer to the model to which this elem belongs to
     @param parentItem address of the parent if the child is not orphan
     @param id of the newly created item. If left to -1, the id is assigned automatically
     @return a ptr to the constructed item
    */
    static std::shared_ptr<TreeItem> construct(const QList<QVariant> &data, std::shared_ptr<AbstractTreeModel> model, std::shared_ptr<TreeItem> parent, int id = -1);

    friend class AbstractTreeModel;
protected:
    // This is protected. Call construct instead
    explicit TreeItem(const QList<QVariant> &data, std::shared_ptr<AbstractTreeModel> model, std::shared_ptr<TreeItem> parent, int id = -1);

public:
    virtual ~TreeItem();

    /* @brief Creates a child of the current item
       @param data: List of data elements (columns) to init the child with.
    */
    std::shared_ptr<TreeItem> appendChild(const QList<QVariant> &data);

    /* @brief Appends an already created child
       Useful for example if the child should be a subclass of TreeItem
    */
    void appendChild(std::shared_ptr<TreeItem> child);

    /* @brief Remove given child from children list. The parent of the child is updated
       accordingly
     */
    void removeChild(std::shared_ptr<TreeItem> child);

    /* @brief Change the parent of the current item. Structures are modified accordingly
     */
    void changeParent(std::shared_ptr<TreeItem> newParent);

    /* @brief Retrieves a child of the current item
       @param row is the index of the child to retrieve
    */
    std::shared_ptr<TreeItem> child(int row) const;

    /* @brief Return the number of children */
    int childCount() const;

    /* @brief Return the number of data fields (columns) */
    int columnCount() const;

    /* @brief Return the content of a column
       @param column Index of the column to look-up
    */
    QVariant dataColumn(int column) const;

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
protected:

    /* @brief Finish construction of object given its pointer
       This is a separated function so that it can be called from derived classes */
    static void baseFinishConstruct(std::shared_ptr<TreeItem> self);


    std::list<std::shared_ptr<TreeItem>> m_childItems;
    std::unordered_map<int, std::list<std::shared_ptr<TreeItem>>::iterator>
    m_iteratorTable; // this logs the iterator associated which each child id. This allows easy access of a child based on its id.

    QList<QVariant> m_itemData;
    std::weak_ptr<TreeItem> m_parentItem;

    std::weak_ptr<AbstractTreeModel> m_model;
    int m_depth;
    int m_id;
};

#endif
