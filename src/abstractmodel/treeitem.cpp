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
#include <QDebug>
#include <numeric>
#include <utility>

TreeItem::TreeItem(QList<QVariant> data, const std::shared_ptr<AbstractTreeModel> &model, bool isRoot, int id)
    : m_itemData(std::move(data))
    , m_model(model)
    , m_depth(0)
    , m_id(id == -1 ? AbstractTreeModel::getNextId() : id)
    , m_isInModel(false)
    , m_isRoot(isRoot)
{
}

std::shared_ptr<TreeItem> TreeItem::construct(const QList<QVariant> &data, std::shared_ptr<AbstractTreeModel> model, bool isRoot, int id)
{
    std::shared_ptr<TreeItem> self(new TreeItem(data, model, isRoot, id));
    baseFinishConstruct(self);
    return self;
}

// static
void TreeItem::baseFinishConstruct(const std::shared_ptr<TreeItem> &self)
{
    if (self->m_isRoot) {
        registerSelf(self);
    }
}

TreeItem::~TreeItem()
{
    deregisterSelf();
}

std::shared_ptr<TreeItem> TreeItem::appendChild(const QList<QVariant> &data)
{
    if (auto ptr = m_model.lock()) {
        auto child = construct(data, ptr, false);
        appendChild(child);
        return child;
    }
    qDebug() << "ERROR: Something went wrong when appending child in TreeItem. Model is not available anymore";
    Q_ASSERT(false);
    return std::shared_ptr<TreeItem>();
}

bool TreeItem::appendChild(const std::shared_ptr<TreeItem> &child)
{
    if (hasAncestor(child->getId())) {
        // in that case, we are trying to create a cycle, abort
        return false;
    }
    if (auto oldParent = child->parentItem().lock()) {
        if (oldParent->getId() == m_id) {
            // no change needed
            return true;
        } else {
            // in that case a call to removeChild should have been carried out
            qDebug() << "ERROR: trying to append a child that alrealdy has a parent";
            return false;
        }
    }
    if (auto ptr = m_model.lock()) {
        ptr->notifyRowAboutToAppend(shared_from_this());
        child->updateParent(shared_from_this());
        int id = child->getId();
        auto it = m_childItems.insert(m_childItems.end(), child);
        m_iteratorTable[id] = it;
        registerSelf(child);
        ptr->notifyRowAppended(child);
        return true;
    }
    qDebug() << "ERROR: Something went wrong when appending child in TreeItem. Model is not available anymore";
    Q_ASSERT(false);
    return false;
}

void TreeItem::moveChild(int ix, const std::shared_ptr<TreeItem> &child)
{
    if (auto ptr = m_model.lock()) {
        auto parentPtr = child->m_parentItem.lock();
        if (parentPtr && parentPtr->getId() != m_id) {
            parentPtr->removeChild(child);
        } else {
            // deletion of child
            auto it = m_iteratorTable[child->getId()];
            m_childItems.erase(it);
        }
        ptr->notifyRowAboutToAppend(shared_from_this());
        child->updateParent(shared_from_this());
        int id = child->getId();
        auto pos = m_childItems.begin();
        std::advance(pos, ix);
        auto it = m_childItems.insert(pos, child);
        m_iteratorTable[id] = it;
        ptr->notifyRowAppended(child);
        m_isInModel = true;
    } else {
        qDebug() << "ERROR: Something went wrong when moving child in TreeItem. Model is not available anymore";
        Q_ASSERT(false);
    }
}

void TreeItem::removeChild(const std::shared_ptr<TreeItem> &child)
{
    if (auto ptr = m_model.lock()) {
        ptr->notifyRowAboutToDelete(shared_from_this(), child->row());
        // get iterator corresponding to child
        Q_ASSERT(m_iteratorTable.count(child->getId()) > 0);
        auto it = m_iteratorTable[child->getId()];
        // deletion of child
        m_childItems.erase(it);
        // clean iterator table
        m_iteratorTable.erase(child->getId());
        child->m_depth = 0;
        child->m_parentItem.reset();
        child->deregisterSelf();
        ptr->notifyRowDeleted();
    } else {
        qDebug() << "ERROR: Something went wrong when removing child in TreeItem. Model is not available anymore";
        Q_ASSERT(false);
    }
}

bool TreeItem::changeParent(std::shared_ptr<TreeItem> newParent)
{
    Q_ASSERT(!m_isRoot);
    if (m_isRoot) return false;
    std::shared_ptr<TreeItem> oldParent;
    if ((oldParent = m_parentItem.lock())) {
        oldParent->removeChild(shared_from_this());
    }
    bool res = true;
    if (newParent) {
        res = newParent->appendChild(shared_from_this());
        if (res) {
            m_parentItem = newParent;
        } else if (oldParent) {
            // something went wrong, we have to reset the parent.
            bool reverse = oldParent->appendChild(shared_from_this());
            Q_ASSERT(reverse);
        }
    }
    return res;
}

std::shared_ptr<TreeItem> TreeItem::child(int row) const
{
    Q_ASSERT(row >= 0 && row < (int)m_childItems.size());
    auto it = m_childItems.cbegin();
    std::advance(it, row);
    return (*it);
}

int TreeItem::childCount() const
{
    return (int)m_childItems.size();
}

int TreeItem::columnCount() const
{
    return m_itemData.count();
}

QVariant TreeItem::dataColumn(int column) const
{
    return m_itemData.value(column);
}

void TreeItem::setData(int column, const QVariant &dataColumn)
{
    m_itemData[column] = dataColumn;
}

std::weak_ptr<TreeItem> TreeItem::parentItem() const
{
    return m_parentItem;
}

int TreeItem::row() const
{
    if (auto ptr = m_parentItem.lock()) {
        // we compute the distance in the parent's children list
        auto it = ptr->m_childItems.begin();
        return (int)std::distance(it, (decltype(it))ptr->m_iteratorTable.at(m_id));
    }
    return -1;
}

int TreeItem::depth() const
{
    return m_depth;
}

int TreeItem::getId() const
{
    return m_id;
}

bool TreeItem::isInModel() const
{
    return m_isInModel;
}

void TreeItem::registerSelf(const std::shared_ptr<TreeItem> &self)
{
    for (const auto &child : self->m_childItems) {
        registerSelf(child);
    }
    if (auto ptr = self->m_model.lock()) {
        ptr->registerItem(self);
        self->m_isInModel = true;
    } else {
        qDebug() << "Error : construction of treeItem failed because parent model is not available anymore";
        Q_ASSERT(false);
    }
}

void TreeItem::deregisterSelf()
{
    for (const auto &child : m_childItems) {
        child->deregisterSelf();
    }
    if (m_isInModel) {
        if (auto ptr = m_model.lock()) {
            ptr->deregisterItem(m_id, this);
            m_isInModel = false;
        }
    }
}

bool TreeItem::hasAncestor(int id)
{
    if (m_id == id) {
        return true;
    }
    if (auto ptr = m_parentItem.lock()) {
        return ptr->hasAncestor(id);
    }
    return false;
}

bool TreeItem::isRoot() const
{
    return m_isRoot;
}

void TreeItem::updateParent(std::shared_ptr<TreeItem> parent)
{
    m_parentItem = parent;
    if (parent) {
        m_depth = parent->m_depth + 1;
    }
}

std::vector<std::shared_ptr<TreeItem>> TreeItem::getLeaves()
{
    if (childCount() == 0) {
        return {shared_from_this()};
    }
    std::vector<std::shared_ptr<TreeItem>> leaves;
    for (const auto &c : m_childItems) {
        for (const auto &l : c->getLeaves()) {
            leaves.push_back(l);
        }
    }
    return leaves;
}
