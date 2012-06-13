/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "projectitemmodel.h"
#include "project/binmodel.h"
#include "project/projectfolder.h"
#include <KLocale>
#include <QItemSelectionModel>

#include <KDebug>


ProjectItemModel::ProjectItemModel(BinModel* binModel, QObject* parent) :
    QAbstractItemModel(parent),
    m_binModel(binModel)
{
    m_selection = new QItemSelectionModel(this);
    connect(m_selection, SIGNAL(currentRowChanged(QModelIndex, QModelIndex)), this, SLOT(onCurrentRowChanged(QModelIndex, QModelIndex)));

    connect(m_binModel, SIGNAL(aboutToAddItem(AbstractProjectItem*)), this, SLOT(onAboutToAddItem(AbstractProjectItem*)));
    connect(m_binModel, SIGNAL(itemAdded(AbstractProjectItem*)), this, SLOT(onItemAdded(AbstractProjectItem*)));
    connect(m_binModel, SIGNAL(aboutToRemoveItem(AbstractProjectItem*)), this, SLOT(onAboutToRemoveItem(AbstractProjectItem*)));
    connect(m_binModel, SIGNAL(itemRemoved(AbstractProjectItem*)), this, SLOT(onItemRemoved(AbstractProjectItem*)));
}

ProjectItemModel::~ProjectItemModel()
{
}

QItemSelectionModel* ProjectItemModel::selectionModel()
{
    return m_selection;
}

QVariant ProjectItemModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        return item->data(static_cast<AbstractProjectItem::DataType>(index.column()));
    }

    return QVariant();
}


Qt::ItemFlags ProjectItemModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return 0;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant ProjectItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        QVariant columnName;
        switch ((AbstractProjectItem::DataType)section) {
            case AbstractProjectItem::DataName:
                columnName = i18n("Name");
                break;
            case AbstractProjectItem::DataDescription:
                columnName = i18n("Description");
                break;
            case AbstractProjectItem::DataDate:
                columnName = i18n("Date");
                break;
        }
        return columnName;
    }

    return QVariant();
}

QModelIndex ProjectItemModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    AbstractProjectItem *parentItem;

    if (parent.isValid()) {
        parentItem = static_cast<AbstractProjectItem*>(parent.internalPointer());
    } else {
        parentItem = m_binModel->rootFolder();
    }

    AbstractProjectItem *childItem = parentItem->at(row);
    return createIndex(row, column, childItem);
}

QModelIndex ProjectItemModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    AbstractProjectItem * parentItem = static_cast<AbstractProjectItem*>(index.internalPointer())->parent();

    if (!parentItem || parentItem == m_binModel->rootFolder()) {
        return QModelIndex();
    }

    return createIndex(parentItem->index(), 0, parentItem);
}

int ProjectItemModel::rowCount(const QModelIndex& parent) const
{
    // ?
    if (parent.column() > 0) {
        return 0;
    }

    AbstractProjectItem *parentItem;
    if (parent.isValid()) {
        parentItem = static_cast<AbstractProjectItem*>(parent.internalPointer());
    } else {
        parentItem = m_binModel->rootFolder();
    }

    return parentItem->count();
}

int ProjectItemModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return static_cast<AbstractProjectItem*>(parent.internalPointer())->supportedDataCount();
    } else {
        return m_binModel->rootFolder()->supportedDataCount();
    }
}

void ProjectItemModel::onCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous)

    if (current.isValid()) {
        AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(current.internalPointer());
        currentItem->setCurrent(true);
    }
}

void ProjectItemModel::onAboutToAddItem(AbstractProjectItem* item)
{
    AbstractProjectItem *parentItem = item->parent();
    QModelIndex parentIndex;
    if (parentItem != m_binModel->rootFolder()) {
        parentIndex = createIndex(parentItem->index(), 0, parentItem);
    }

    beginInsertRows(parentIndex, parentItem->count(), parentItem->count());
}

void ProjectItemModel::onItemAdded(AbstractProjectItem* item)
{
    Q_UNUSED(item)

    endInsertRows();
}

void ProjectItemModel::onAboutToRemoveItem(AbstractProjectItem* item)
{
    AbstractProjectItem *parentItem = item->parent();
    QModelIndex parentIndex;
    if (parentItem != m_binModel->rootFolder()) {
        parentIndex = createIndex(parentItem->index(), 0, parentItem);
    }

    beginRemoveRows(parentIndex, item->index(), item->index());
}

void ProjectItemModel::onItemRemoved(AbstractProjectItem* item)
{
    Q_UNUSED(item)

    endRemoveRows();
}

#include "projectitemmodel.moc"
