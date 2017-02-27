/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "projectitemmodel.h"
#include "abstractprojectitem.h"
#include "projectclip.h"
#include "projectsubclip.h"
#include "projectfolder.h"
#include "bin.h"

#include <qvarlengtharray.h>
#include <KLocalizedString>
#include <QIcon>
#include <QMimeData>

ProjectItemModel::ProjectItemModel(Bin *bin) :
    QAbstractItemModel(bin)
    , m_bin(bin)
{
    connect(m_bin, &Bin::itemUpdated, this, &ProjectItemModel::onItemUpdated);
}

ProjectItemModel::~ProjectItemModel()
{
}

int ProjectItemModel::mapToColumn(int column) const
{
    switch (column) {
    case 0:
        return AbstractProjectItem::DataName;
        break;
    case 1:
        return AbstractProjectItem::DataDate;
        break;
    case 2:
        return AbstractProjectItem::DataDescription;
        break;
    default:
        return AbstractProjectItem::DataName;
    }
}

QVariant ProjectItemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        return item->data(static_cast<AbstractProjectItem::DataType>(mapToColumn(index.column())));
    }
    if (role == Qt::DecorationRole) {
        if (index.column() != 0) {
            return QVariant();
        }
        // Data has to be returned as icon to allow the view to scale it
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        QIcon icon = item->data(AbstractProjectItem::DataThumbnail).value<QIcon>();
        return icon;
    } else {
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        return item->data((AbstractProjectItem::DataType) role);
    }
}

bool ProjectItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
    if (item->rename(value.toString(), index.column())) {
        emit dataChanged(index, index, QVector<int> () << role);
        return true;
    }
    // Item name was not changed
    return false;
}

Qt::ItemFlags ProjectItemModel::flags(const QModelIndex &index) const
{
    /*return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;*/
    if (!index.isValid()) {
        return Qt::ItemIsDropEnabled;
    }
    AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
    AbstractProjectItem::PROJECTITEMTYPE type = item->itemType();
    switch (type) {
    case AbstractProjectItem::FolderItem:
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
        break;
    case AbstractProjectItem::ClipItem:
        if (!item->statusReady()) {
            return Qt::ItemIsSelectable;
        }
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
        break;
    case AbstractProjectItem::SubClipItem:
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
        break;
    case AbstractProjectItem::FolderUpItem:
        return Qt::ItemIsEnabled;
        break;
    default:
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }
}

// cppcheck-suppress unusedFunction
bool ProjectItemModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(row)
    Q_UNUSED(column)
    if (action == Qt::IgnoreAction) {
        return true;
    }

    if (data->hasUrls()) {
        emit itemDropped(data->urls(), parent);
        return true;
    }

    if (data->hasFormat(QStringLiteral("kdenlive/producerslist"))) {
        // Dropping an Bin item
        QStringList ids = QString(data->data(QStringLiteral("kdenlive/producerslist"))).split(QLatin1Char(';'));
        emit itemDropped(ids, parent);
        return true;
    }

    if (data->hasFormat(QStringLiteral("kdenlive/effectslist"))) {
        // Dropping effect on a Bin item
        const QString effect = QString::fromUtf8(data->data(QStringLiteral("kdenlive/effectslist")));
        emit effectDropped(effect, parent);
        return true;
    }

    if (data->hasFormat(QStringLiteral("kdenlive/clip"))) {
        QStringList list = QString(data->data(QStringLiteral("kdenlive/clip"))).split(QLatin1Char(';'));
        emit addClipCut(list.at(0), list.at(1).toInt(), list.at(2).toInt());
    }

    return false;
}

QVariant ProjectItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        QVariant columnName;
        switch (section) {
        case 0:
            columnName = i18n("Name");
            break;
        case 1:
            columnName = i18n("Date");
            break;
        case 2:
            columnName = i18n("Description");
            break;
        default:
            columnName = i18n("Unknown");
            break;
        }
        return columnName;
    }
    return QAbstractItemModel::headerData(section, orientation, role);
}

QModelIndex ProjectItemModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    AbstractProjectItem *parentItem;

    if (parent.isValid()) {
        parentItem = static_cast<AbstractProjectItem *>(parent.internalPointer());
    } else {
        parentItem = m_bin->rootFolder();
    }

    AbstractProjectItem *childItem = parentItem->at(row);
    return createIndex(row, column, childItem);
}

QModelIndex ProjectItemModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    AbstractProjectItem *parentItem = static_cast<AbstractProjectItem *>(index.internalPointer())->parent();

    if (!parentItem || parentItem == m_bin->rootFolder()) {
        return QModelIndex();
    }

    return createIndex(parentItem->index(), 0, parentItem);
}

int ProjectItemModel::rowCount(const QModelIndex &parent) const
{
    // ?
    /*if (parent.column() > 0) {
        return 0;
    }*/

    AbstractProjectItem *parentItem;
    if (parent.isValid()) {
        parentItem = static_cast<AbstractProjectItem *>(parent.internalPointer());
    } else {
        parentItem = m_bin->rootFolder();
    }

    return parentItem->count();
}

int ProjectItemModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return static_cast<AbstractProjectItem *>(parent.internalPointer())->supportedDataCount();
    } else {
        return m_bin->rootFolder()->supportedDataCount();
    }
}

// cppcheck-suppress unusedFunction
Qt::DropActions ProjectItemModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList ProjectItemModel::mimeTypes() const
{
    QStringList types;
    types << QStringLiteral("kdenlive/producerslist") << QStringLiteral("text/uri-list") << QStringLiteral("kdenlive/clip") << QStringLiteral("kdenlive/effectslist");
    return types;
}

QMimeData *ProjectItemModel::mimeData(const QModelIndexList &indices) const
{
    // Mime data is a list of id's separated by ';'.
    // Clip ids are represented like:  2 (where 2 is the clip's id)
    // Clip zone ids are represented like:  2/10/200 (where 2 is the clip's id, 10 and 200 are in and out points)
    // Folder ids are represented like:  #2 (where 2 is the folder's id)
    QMimeData *mimeData = new QMimeData();
    QStringList list;
    for (int i = 0; i < indices.count(); i++) {
        QModelIndex ix = indices.at(i);
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(ix.internalPointer());
        AbstractProjectItem::PROJECTITEMTYPE type = item->itemType();
        if (type == AbstractProjectItem::ClipItem) {
            list << item->clipId();
        } else if (type == AbstractProjectItem::SubClipItem) {
            QPoint p = item->zone();
            list << item->clipId() + QLatin1Char('/') + QString::number(p.x()) + QLatin1Char('/') + QString::number(p.y());
        } else if (type == AbstractProjectItem::FolderItem) {
            list << "#" + item->clipId();
        }
    }
    if (!list.isEmpty()) {
        QByteArray data;
        data.append(list.join(QLatin1Char(';')).toUtf8());
        mimeData->setData(QStringLiteral("kdenlive/producerslist"),  data);
    }
    return mimeData;
}

void ProjectItemModel::onAboutToAddItem(AbstractProjectItem *item)
{
    AbstractProjectItem *parentItem = item->parent();
    if (parentItem == nullptr) {
        return;
    }
    QModelIndex parentIndex;
    if (parentItem != m_bin->rootFolder()) {
        parentIndex = createIndex(parentItem->index(), 0, parentItem);
    }
    beginInsertRows(parentIndex, parentItem->count(), parentItem->count());
}

void ProjectItemModel::onItemAdded(AbstractProjectItem *item)
{
    Q_UNUSED(item)
    endInsertRows();
}

void ProjectItemModel::onAboutToRemoveItem(AbstractProjectItem *item)
{
    AbstractProjectItem *parentItem = item->parent();
    if (parentItem == nullptr) {
        return;
    }
    QModelIndex parentIndex;
    if (parentItem != m_bin->rootFolder()) {
        parentIndex = createIndex(parentItem->index(), 0, parentItem);
    }

    beginRemoveRows(parentIndex, item->index(), item->index());
}

void ProjectItemModel::onItemRemoved(AbstractProjectItem *item)
{
    Q_UNUSED(item)
    endRemoveRows();
}

void ProjectItemModel::onItemUpdated(AbstractProjectItem *item)
{
    if (!item || item->clipStatus() == AbstractProjectItem::StatusDeleting) {
        return;
    }
    AbstractProjectItem *parentItem = item->parent();
    if (parentItem == nullptr) {
        return;
    }
    QModelIndex parentIndex;
    if (parentItem != m_bin->rootFolder()) {
        parentIndex = createIndex(parentItem->index(), 0, parentItem);
    }
    emit dataChanged(parentIndex, parentIndex);
}
