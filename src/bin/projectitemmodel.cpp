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
#include "projectfolder.h"
#include "bin.h"

#include <qvarlengtharray.h>
#include <KLocalizedString>
#include <QItemSelectionModel>
#include <QIcon>
#include <QMimeData>
#include <QDebug>
#include <QStringListModel>


ProjectItemModel::ProjectItemModel(Bin *bin) :
    QAbstractItemModel(bin)
  , m_bin(bin)
  , m_iconSize(160, 90)
{
    connect(m_bin, SIGNAL(itemUpdated(AbstractProjectItem*)), this, SLOT(onItemUpdated(AbstractProjectItem*)));
    //connect(m_bin, SIGNAL(markersNeedUpdate(QString,QList<int>)), this, SIGNAL(markersNeedUpdate(QString,QList<int>)));*/
}

ProjectItemModel::~ProjectItemModel()
{
}


void ProjectItemModel::setIconSize(QSize s)
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
    }
    if (role == Qt::DecorationRole && index.column() == 0) {
        // Data has to be returned as icon to allow the view to scale it
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        QIcon icon = QIcon(item->data(AbstractProjectItem::DataThumbnail).value<QPixmap>());
        if (icon.isNull()) {
            QPixmap pix(m_iconSize);
            pix.fill(Qt::lightGray);
            icon = QIcon(pix);
        }
        return icon;
    } 
    if (role == Qt::UserRole) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        return item->data(AbstractProjectItem::DataDuration);
    }
    if (role == AbstractProjectItem::JobType || role == AbstractProjectItem::JobProgress || role ==  AbstractProjectItem::JobMessage || role ==  AbstractProjectItem::ItemTypeRole || role == AbstractProjectItem::ClipStatus) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        return item->data((AbstractProjectItem::DataType) role);
    }

    return QVariant();
}

Qt::ItemFlags ProjectItemModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::ItemIsDropEnabled;
    }
    AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
    if (item->isFolder()) return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

bool ProjectItemModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;

    if (data->hasUrls()) {
        emit itemDropped(data->urls(), parent);
        return true;
    }

    if (data->hasFormat("kdenlive/producerslist")) {
        // Dropping an Bin item
        QStringList ids = QString(data->data("kdenlive/producerslist")).split(';');
        emit itemDropped(ids, parent);
        return true;
    }

    return false;
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
        parentItem = m_bin->rootFolder();
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

    if (!parentItem || parentItem == m_bin->rootFolder()) {
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
        parentItem = m_bin->rootFolder();
    }

    return parentItem->count();
}

int ProjectItemModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return static_cast<AbstractProjectItem*>(parent.internalPointer())->supportedDataCount();
    } else {
        return m_bin->rootFolder()->supportedDataCount();
    }
}

Qt::DropActions ProjectItemModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList ProjectItemModel::mimeTypes() const
{
    QStringList types;
    types << QLatin1String("kdenlive/producerslist") << QLatin1String("text/uri-list");
    return types;
}

QMimeData* ProjectItemModel::mimeData(const QModelIndexList& indices) const
{
    QMimeData *mimeData = new QMimeData();
    if (indices.count() >= 1 && indices.at(0).isValid()) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem*>(indices.at(0).internalPointer());
        ProjectClip *clip = qobject_cast<ProjectClip*>(item);
        if (clip) {
            QStringList list;
            list << clip->clipId();
            QByteArray data;
            data.append(list.join(QLatin1String(";")).toUtf8());
            mimeData->setData(QLatin1String("kdenlive/producerslist"),  data);
            return mimeData;
        }
    }
}


void ProjectItemModel::onAboutToAddItem(AbstractProjectItem* item)
{
    AbstractProjectItem *parentItem = item->parent();
    QModelIndex parentIndex;
    if (parentItem != m_bin->rootFolder()) {
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
    if (parentItem != m_bin->rootFolder()) {
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
    if (parentItem != m_bin->rootFolder()) {
        parentIndex = createIndex(parentItem->index(), 0, parentItem);
    }
    emit dataChanged(parentIndex, parentIndex);
}


