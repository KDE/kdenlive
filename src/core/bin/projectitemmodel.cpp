/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "projectitemmodel.h"
#include "project/project.h"
#include "project/projectfolder.h"
#include <KLocale>


ProjectItemModel::ProjectItemModel(Project* project, QObject* parent) :
    QAbstractItemModel(parent),
    m_project(project)
{
    
}

ProjectItemModel::~ProjectItemModel()
{
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
        parentItem = m_project->items();
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

    if (parentItem == m_project->items()) {
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
        parentItem = m_project->items();
    }

    return parentItem->count();
}

int ProjectItemModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return static_cast<AbstractProjectItem*>(parent.internalPointer())->supportedDataCount();
    } else {
        return m_project->items()->supportedDataCount();
    }
}

#include "projectitemmodel.moc"
