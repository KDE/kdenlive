/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "abstracttreemodel.hpp"

#include "treeitem.hpp"
#include <QDebug>
#include <utility>

#include <queue>
#include <unordered_set>

int AbstractTreeModel::currentTreeId = 0;
AbstractTreeModel::AbstractTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

std::shared_ptr<AbstractTreeModel> AbstractTreeModel::construct(QObject *parent)
{
    std::shared_ptr<AbstractTreeModel> self(new AbstractTreeModel(parent));
    self->rootItem = TreeItem::construct(QList<QVariant>(), self, true);
    return self;
}

AbstractTreeModel::~AbstractTreeModel()
{
    m_allItems.clear();
    rootItem.reset();
}

int AbstractTreeModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) return rootItem->columnCount();

    const auto id = int(parent.internalId());
    auto item = getItemById(id);
    return item->columnCount();
}

QVariant AbstractTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    auto item = getItemById(int(index.internalId()));
    return item->dataColumn(index.column());
}

Qt::ItemFlags AbstractTreeModel::flags(const QModelIndex &index) const
{
    const auto flags = QAbstractItemModel::flags(index);
    if (index.isValid()) {
        auto item = getItemById(int(index.internalId()));
        if (item->depth() == 1) {
            return flags & ~Qt::ItemIsSelectable;
        }
    }
    return flags;
}

QVariant AbstractTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) return rootItem->dataColumn(section);

    return QVariant();
}

QModelIndex AbstractTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    std::shared_ptr<TreeItem> parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = getItemById(int(parent.internalId()));

    if (row >= parentItem->childCount()) return QModelIndex();

    std::shared_ptr<TreeItem> childItem = parentItem->child(row);
    if (childItem) return createIndex(row, column, quintptr(childItem->getId()));

    return {};
}

QModelIndex AbstractTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return {};

    std::shared_ptr<TreeItem> childItem = getItemById(int(index.internalId()));
    std::shared_ptr<TreeItem> parentItem = childItem->parentItem().lock();

    Q_ASSERT(parentItem);

    if (parentItem == rootItem) return QModelIndex();

    return createIndex(parentItem->row(), 0, quintptr(parentItem->getId()));
}

int AbstractTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;

    std::shared_ptr<TreeItem> parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = getItemById(int(parent.internalId()));

    return parentItem->childCount();
}

QModelIndex AbstractTreeModel::getIndexFromItem(const std::shared_ptr<TreeItem> &item, int column) const
{
    if (item == rootItem) {
        return QModelIndex();
    }
    if (auto ptr = item->parentItem().lock()) {
        auto parentIndex = getIndexFromItem(ptr);
        return index(item->row(), column, parentIndex);
    }
    return QModelIndex();
}

QModelIndex AbstractTreeModel::getIndexFromId(int id) const
{
    if (id == rootItem->getId()) {
        return QModelIndex();
    }
    Q_ASSERT(m_allItems.count(id) > 0);
    if (auto ptr = m_allItems.at(id).lock()) return getIndexFromItem(ptr);

    Q_ASSERT(false);
    return {};
}

void AbstractTreeModel::notifyRowAboutToAppend(const std::shared_ptr<TreeItem> &item)
{
    auto index = getIndexFromItem(item);
    beginInsertRows(index, item->childCount(), item->childCount());
}

void AbstractTreeModel::notifyRowAppended(const std::shared_ptr<TreeItem> &row)
{
    Q_UNUSED(row);
    endInsertRows();
}

void AbstractTreeModel::notifyRowAboutToDelete(std::shared_ptr<TreeItem> item, int row)
{
    auto index = getIndexFromItem(item);
    beginRemoveRows(index, row, row);
}

void AbstractTreeModel::notifyRowDeleted()
{
    endRemoveRows();
}

// static
int AbstractTreeModel::getNextId()
{
    return currentTreeId++;
}

void AbstractTreeModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    int id = item->getId();
    Q_ASSERT(m_allItems.count(id) == 0);
    m_allItems[id] = item;
}

void AbstractTreeModel::deregisterItem(int id, TreeItem *item)
{
    Q_UNUSED(item);
    Q_ASSERT(m_allItems.count(id) > 0);
    m_allItems.erase(id);
}

std::shared_ptr<TreeItem> AbstractTreeModel::getItemById(int id) const
{
    if (id == rootItem->getId()) {
        return rootItem;
    }
    Q_ASSERT(m_allItems.count(id) > 0);
    return m_allItems.at(id).lock();
}

std::shared_ptr<TreeItem> AbstractTreeModel::getRoot() const
{
    return rootItem;
}

bool AbstractTreeModel::checkConsistency()
{
    // first check that the root is all good
    if (!rootItem || !rootItem->m_isRoot || !rootItem->isInModel() || m_allItems.count(rootItem->getId()) == 0) {
        qCCritical(KDENLIVE_LOG) << "AbstractTreeModel is not valid because root is not properly constructed";
        return false;
    }
    // Then we traverse the tree from the root, checking the infos on the way
    std::unordered_set<int> seenIDs;
    std::queue<std::pair<int, std::pair<int, int>>> queue; // store (id, (depth, parentId))
    queue.push({rootItem->getId(), {0, rootItem->getId()}});
    while (!queue.empty()) {
        auto current = queue.front();
        int currentId = current.first, currentDepth = current.second.first;
        int parentId = current.second.second;
        queue.pop();
        if (seenIDs.count(currentId) != 0) {
            qCCritical(KDENLIVE_LOG) << "Invalid tree: Id found twice."
                                     << "It either a cycle or a clash in id attribution";
            return false;
        }
        if (m_allItems.count(currentId) == 0) {
            qCCritical(KDENLIVE_LOG) << "Invalid tree: Id not found. Item is not registered";
            return false;
        }
        auto currentItem = m_allItems[currentId].lock();
        if (currentItem->depth() != currentDepth) {
            qCCritical(KDENLIVE_LOG) << "Invalid tree: invalid depth info found";
            return false;
        }
        if (!currentItem->isInModel()) {
            qCCritical(KDENLIVE_LOG) << "Invalid tree: item thinks it is not in a model";
            return false;
        }
        if (currentId != rootItem->getId()) {
            if ((currentDepth == 0 || currentItem->m_isRoot)) {
                qCCritical(KDENLIVE_LOG) << "Invalid tree: duplicate root";
                return false;
            }
            if (auto ptr = currentItem->parentItem().lock()) {
                if (ptr->getId() != parentId || ptr->child(currentItem->row())->getId() != currentItem->getId()) {
                    qCCritical(KDENLIVE_LOG) << "Invalid tree: invalid parent link";
                    return false;
                }
            } else {
                qCCritical(KDENLIVE_LOG) << "Invalid tree: invalid parent";
                return false;
            }
        }
        // propagate to children
        int i = 0;
        for (const auto &child : currentItem->m_childItems) {
            if (currentItem->child(i) != child) {
                qCCritical(KDENLIVE_LOG) << "Invalid tree: invalid child ordering";
                return false;
            }
            queue.push({child->getId(), {currentDepth + 1, currentId}});
            i++;
        }
    }
    return true;
}

Fun AbstractTreeModel::addItem_lambda(const std::shared_ptr<TreeItem> &new_item, int parentId)
{
    return [this, new_item, parentId]() {
        if (new_item->m_isInvalid) {
            return true;
        }
        /* Insertion is simply setting the parent of the item.*/
        std::shared_ptr<TreeItem> parent;
        if (parentId != -1) {
            parent = getItemById(parentId);
            if (!parent) {
                Q_ASSERT(parent);
                return false;
            }
        }
        return new_item->changeParent(parent);
    };
}

Fun AbstractTreeModel::removeItem_lambda(int id)
{
    return [this, id]() {
        /* Deletion simply deregister clip and remove it from parent.
           The actual object is not actually deleted, because a shared_pointer to it
           is captured by the reverse operation.
           Actual deletions occurs when the undo object is destroyed.
        */
        if (m_allItems.count(id) == 0) {
            // Invalid item, might have been deleted on insert
            return true;
        }
        auto item = m_allItems[id].lock();
        Q_ASSERT(item);
        if (!item) {
            return false;
        }
        auto parent = item->parentItem().lock();
        parent->removeChild(item);
        return true;
    };
}

Fun AbstractTreeModel::moveItem_lambda(int id, int destRow, bool force)
{
    Fun lambda = []() { return true; };
    std::vector<std::shared_ptr<TreeItem>> oldStack;
    auto item = getItemById(id);
    if (!force && item->row() == destRow) {
        // nothing to do
        return lambda;
    }
    if (auto parent = item->parentItem().lock()) {
        if (destRow > parent->childCount() || destRow < 0) {
            return []() { return false; };
        }
        int parentId = parent->getId();
        // remove the element to move
        oldStack.push_back(item);
        Fun oper = removeItem_lambda(id);
        PUSH_LAMBDA(oper, lambda);
        // remove the tail of the stack
        for (int i = destRow; i < parent->childCount(); ++i) {
            auto current = parent->child(i);
            if (current->getId() != id) {
                oldStack.push_back(current);
                oper = removeItem_lambda(current->getId());
                PUSH_LAMBDA(oper, lambda);
            }
        }
        // insert back in order
        for (const auto &elem : oldStack) {
            oper = addItem_lambda(elem, parentId);
            PUSH_LAMBDA(oper, lambda);
        }
        return lambda;
    }
    return []() { return false; };
}
