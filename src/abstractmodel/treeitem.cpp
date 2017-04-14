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

#include "treeitem.hpp"
#include "abstracttreemodel.hpp"

TreeItem::TreeItem(const QList<QVariant> &data, AbstractTreeModel* model, TreeItem *parent)
{
    m_parentItem = parent;
    m_itemData = data;
    m_depth = 0;
    m_model = model;
}

TreeItem::~TreeItem()
{
    qDeleteAll(m_childItems);
}

TreeItem* TreeItem::appendChild(const QList<QVariant> &data)
{
    m_model->notifyRowAboutToAppend(this);
    auto *child = new TreeItem(data, m_model, this);
    child->m_depth = m_depth + 1;
    m_childItems.append(child);
    m_model->notifyRowAppended();
    return child;
}

void TreeItem::appendChild(TreeItem *child)
{
    m_model->notifyRowAboutToAppend(this);
    child->m_depth = m_depth + 1;
    child->m_parentItem = this;
    m_childItems.append(child);
    m_model->notifyRowAppended();
}

void TreeItem::removeChild(TreeItem *child)
{
    m_model->notifyRowAboutToDelete(this, child->row());
    bool success = m_childItems.removeAll(child) != 0;
    Q_ASSERT(success);
    child->m_depth = 0;
    child->m_parentItem = nullptr;
    m_model->notifyRowDeleted();
}

void TreeItem::changeParent(TreeItem *newParent)
{
    if (m_parentItem) {
        m_parentItem->removeChild(this);
    }
    if (newParent) {
        newParent->appendChild(this);
    }
}

TreeItem *TreeItem::child(int row) const
{
    return m_childItems.value(row);
}

int TreeItem::childCount() const
{
    return m_childItems.count();
}

int TreeItem::columnCount() const
{
    return m_itemData.count();
}

QVariant TreeItem::dataColumn(int column) const
{
    return m_itemData.value(column);
}

TreeItem *TreeItem::parentItem()
{
    return m_parentItem;
}

int TreeItem::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<TreeItem*>(this));

    return -1;
}

int TreeItem::depth() const
{
    return m_depth;
}

