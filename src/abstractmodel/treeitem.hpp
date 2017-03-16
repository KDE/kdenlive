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


/* @brief This class is a generic class to represent items of a tree-like model
 */

class TreeItem
{
public:
    /* @brief Construct a TreeItem
     @param data List of data elements (columns) of the created item
     @param parentItem address of the parent if the child is not orphan
    */
    explicit TreeItem(const QList<QVariant> &data, TreeItem *parentItem = nullptr);
    ~TreeItem();

    /* @brief Creates a child of the current item
       @param data: List of data elements (columns) to init the child with.
    */
    TreeItem* appendChild(const QList<QVariant> &data);

    /* @brief Retrieves a child of the current item
       @param row is the index of the child to retrieve
    */
    TreeItem *child(int row);

    /* @brief Return the number of children */
    int childCount() const;

    /* @brief Return the number of data fields (columns) */
    int columnCount() const;

    /* @brief Return the content of a column
       @param column Index of the column to look-up
    */
    QVariant data(int column) const;

    /* @brief Return the index of current item amongst father's children
     */
    int row() const;

    /* @brief Return a ptr to the parent item
     */
    TreeItem *parentItem();

    /* @brief Return the depth of the current item*/
    int depth() const;

private:
    QList<TreeItem*> m_childItems;
    QList<QVariant> m_itemData;
    TreeItem *m_parentItem;
    int m_depth;
};

#endif
