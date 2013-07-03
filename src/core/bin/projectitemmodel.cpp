/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "projectitemmodel.h"
#include <qvarlengtharray.h>
#include "project/binmodel.h"
#include "monitor/mltcontroller.h"
#include "project/projectfolder.h"
#include "project/project.h"
#include "project/abstractprojectclip.h"
#include <KLocale>
#include <QItemSelectionModel>
#include <QIcon>
#include <QMimeData>

#include <KDebug>


ProjectItemModel::ProjectItemModel(BinModel* binModel, QObject* parent) :
    QAbstractItemModel(parent)
  , m_binModel(binModel)
  , m_iconSize(60)
{
    m_selection = new QItemSelectionModel(this);
    //connect(m_selection, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(onCurrentRowChanged(QModelIndex,QModelIndex)));
    connect(m_selection, SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(onCurrentRowChanged(QModelIndex,QModelIndex)));

    connect(m_binModel, SIGNAL(aboutToAddItem(AbstractProjectItem*)), this, SLOT(onAboutToAddItem(AbstractProjectItem*)));
    connect(m_binModel, SIGNAL(itemAdded(AbstractProjectItem*)), this, SLOT(onItemAdded(AbstractProjectItem*)));
    connect(m_binModel, SIGNAL(aboutToRemoveItem(AbstractProjectItem*)), this, SLOT(onAboutToRemoveItem(AbstractProjectItem*)));
    connect(m_binModel, SIGNAL(itemRemoved(AbstractProjectItem*)), this, SLOT(onItemRemoved(AbstractProjectItem*)));
    connect(m_binModel, SIGNAL(itemUpdated(AbstractProjectItem*)), this, SLOT(onItemUpdated(AbstractProjectItem*)));
    connect(m_binModel, SIGNAL(markersNeedUpdate(QString,QList<int>)), this, SIGNAL(markersNeedUpdate(QString,QList<int>)));
}

ProjectItemModel::~ProjectItemModel()
{
}

QItemSelectionModel* ProjectItemModel::selectionModel()
{
    return m_selection;
}

void ProjectItemModel::setIconSize(int s)
{
    m_iconSize = s;
}

QVariant ProjectItemModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        return item->data(static_cast<AbstractProjectItem::DataType>(index.column()));
    } else if (role == Qt::DecorationRole && index.column() == 0) {
        // Data has to be returned as icon to allow the view to scale it
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        QIcon icon = QIcon(item->data(AbstractProjectItem::DataThumbnail).value<QPixmap>());
        if (icon.isNull()) {
            int h = m_iconSize;
            int w = h * m_binModel->project()->profile()->dar();
            QPixmap pix(w, h);
            pix.fill(Qt::lightGray);
            icon = QIcon(pix);
        }
        return icon;
    } else if (role == Qt::UserRole) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        return item->data(AbstractProjectItem::DataDuration);
    }

    return QVariant();
}

Qt::ItemFlags ProjectItemModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return 0;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
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
        default:
            columnName = i18n("Unknown");
            break;
        }
        return columnName;
    }

    return QAbstractItemModel::headerData(section, orientation, role);
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

QStringList ProjectItemModel::mimeTypes() const
{
    QStringList types;
    types << QLatin1String("kdenlive/clip");
    return types;
}

QMimeData* ProjectItemModel::mimeData(const QModelIndexList& indices) const
{
    QMimeData *mimeData = new QMimeData();

    if (indices.count() >= 1 && indices.at(0).isValid()) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem*>(indices.at(0).internalPointer());
        AbstractProjectClip *clip = qobject_cast<AbstractProjectClip*>(item);
        if (clip) {
            QStringList list;
            list << clip->clipId();
            QByteArray data;
            data.append(list.join(QLatin1String(";")).toUtf8());
            mimeData->setData(QLatin1String("kdenlive/clip"),  data);
            return mimeData;
        }
    }

    return 0;
    
}

void ProjectItemModel::onCurrentRowChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous)
    kDebug()<<" + + + + + + + ++ ROW CHANGED+: "<<current.row()<<", PREVIOUS: "<<previous.row();
    if (previous.row() == current.row()) {
        // User clicked on already selected item, don't react?
        return;
    }
    QModelIndex id = index(current.row(), 0, QModelIndex());
    emit selectModel(id);
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
    //emit updateCurrentItem();
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


void ProjectItemModel::onItemUpdated(AbstractProjectItem* item)
{
    AbstractProjectItem *parentItem = item->parent();
    QModelIndex parentIndex;
    if (parentItem != m_binModel->rootFolder()) {
        parentIndex = createIndex(parentItem->index(), 0, parentItem);
    }
    emit dataChanged(parentIndex, parentIndex);
}

#include "projectitemmodel.moc"
