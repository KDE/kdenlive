/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "undohelper.hpp"
#include <QAbstractItemModel>
#include <memory>
#include <unordered_map>

/** @class AbstractTreeModel
    @brief This class represents a generic tree hierarchy
 */
class TreeItem;
class AbstractTreeModel : public QAbstractItemModel, public std::enable_shared_from_this<AbstractTreeModel>
{
    Q_OBJECT

public:
    /** @brief Construct a TreeModel
       @param parent is the parent object of the model
       @return a ptr to the created object
    */
    static std::shared_ptr<AbstractTreeModel> construct(QObject *parent = nullptr);

protected:
    // This is protected. Call construct instead.
    explicit AbstractTreeModel(QObject *parent = nullptr);

public:
    ~AbstractTreeModel() override;

    /** @brief Given an item from the hierarchy, construct the corresponding ModelIndex */
    QModelIndex getIndexFromItem(const std::shared_ptr<TreeItem> &item, int column = 0) const;

    /** @brief Given an item id, construct the corresponding ModelIndex */
    QModelIndex getIndexFromId(int id) const;

    /** @brief Return a ptr to an item given its id */
    std::shared_ptr<TreeItem> getItemById(int id) const;

    /** @brief Return a ptr to the root of the tree */
    std::shared_ptr<TreeItem> getRoot() const;

    QVariant data(const QModelIndex &index, int role) const override;
    // This is reimplemented to prevent selection of the categories
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /** @brief Helper function to generate a lambda that adds an item to the tree */
    Fun addItem_lambda(const std::shared_ptr<TreeItem> &new_item, int parentId);

    /** @brief Helper function to generate a lambda that removes an item from the tree */
    Fun removeItem_lambda(int id);

    /** @brief Helper function to generate a lambda that changes the row of an item */
    Fun moveItem_lambda(int id, int destRow, bool force = false);

    friend class TreeItem;
    friend class AbstractProjectItem;

protected:
    /** @brief Register a new item. This is a call-back meant to be called from TreeItem */
    virtual void registerItem(const std::shared_ptr<TreeItem> &item);

    /** @brief Deregister an item. This is a call-back meant to be called from TreeItem */
    virtual void deregisterItem(int id, TreeItem *item);

    /** @brief Returns the next valid id to give to a new element */
    static int getNextId();

    /** @brief Send the appropriate notification related to a row that we are appending
       @param item is the parent item to which row is appended
    */
    void notifyRowAboutToAppend(const std::shared_ptr<TreeItem> &item);

    /** @brief Send the appropriate notification related to a row that we have appended
       @param row is the new element
    */
    void notifyRowAppended(const std::shared_ptr<TreeItem> &row);

    /** @brief Send the appropriate notification related to a row that we are deleting
       @param item is the parent of the row being deleted
       @param row is the index of the row being deleted
    */
    void notifyRowAboutToDelete(std::shared_ptr<TreeItem> item, int row);

    /** @brief Send the appropriate notification related to a row that we have appended
       @param row is the old element
    */
    void notifyRowDeleted();

    /** @brief This is a convenience function that helps check if the tree is in a valid state */
    virtual bool checkConsistency();

protected:
    std::shared_ptr<TreeItem> rootItem;

    std::unordered_map<int, std::weak_ptr<TreeItem>> m_allItems;

    static int currentTreeId;
};
