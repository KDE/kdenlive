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

#include <qvarlengtharray.h>
#include <KLocalizedString>
#include <QIcon>
#include <QMimeData>

ProjectItemModel::ProjectItemModel(Bin *bin, QObject *parent) :
    AbstractTreeModel(parent)
    , m_bin(bin)
{
    rootItem = new ProjectFolder(this);
}

ProjectItemModel::~ProjectItemModel()
{
    delete rootItem;
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
        auto type = static_cast<AbstractProjectItem::DataType>(mapToColumn(index.column()));
        QVariant ret = item->getData(type);
        return ret;
    }
    if (role == Qt::DecorationRole) {
        if (index.column() != 0) {
            return QVariant();
        }
        // Data has to be returned as icon to allow the view to scale it
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        QVariant thumb = item->getData(AbstractProjectItem::DataThumbnail);
        QIcon icon;
        if (thumb.canConvert<QIcon>()) {
            icon = thumb.value<QIcon>();
        } else {
            qDebug() << "ERROR: invalid icon found";
        }
        return icon;
    } else {
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(index.internalPointer());
        return item->getData((AbstractProjectItem::DataType) role);
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


int ProjectItemModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return static_cast<AbstractProjectItem *>(parent.internalPointer())->supportedDataCount();
    } else {
        return static_cast<ProjectFolder*>(rootItem)->supportedDataCount();
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
    int duration = 0;
    for (int i = 0; i < indices.count(); i++) {
        QModelIndex ix = indices.at(i);
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        AbstractProjectItem *item = static_cast<AbstractProjectItem *>(ix.internalPointer());
        AbstractProjectItem::PROJECTITEMTYPE type = item->itemType();
        if (type == AbstractProjectItem::ClipItem) {
            list << item->clipId();
            duration += ((ProjectClip *)(item))->frameDuration();
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
        qDebug()<<"/// CLI DURATION: "<<duration;
        mimeData->setText(QString::number(duration));
    }
    return mimeData;
}


void ProjectItemModel::onItemUpdated(AbstractProjectItem *item)
{
    auto index = getIndexFromItem(static_cast<TreeItem*>(item));
    emit dataChanged(index, index);
}

ProjectClip *ProjectItemModel::getClipByBinID(const QString& binId)
{
    return static_cast<AbstractProjectItem*>(rootItem)->clip(binId);
}

ProjectFolder *ProjectItemModel::getFolderByBinId(const QString& binId)
{
    return static_cast<AbstractProjectItem*>(rootItem)->folder(binId);
}

QStringList ProjectItemModel::getEnclosingFolderInfo(const QModelIndex& index) const
{
    QStringList noInfo;
    noInfo << QString::number(-1);
    noInfo << QString();
    if (!index.isValid()) {
        return noInfo;
    }

    AbstractProjectItem *currentItem = static_cast<AbstractProjectItem *>(index.internalPointer());
    auto folder = currentItem->getEnclosingFolder(true);
    if (!folder || folder == rootItem) {
        return noInfo;
    } else {
        QStringList folderInfo;
        folderInfo << currentItem->clipId();
        folderInfo << currentItem->name();
        return folderInfo;
    }

}

void ProjectItemModel::clean()
{
    delete rootItem;
    rootItem = new ProjectFolder(this);
}


ProjectFolder *ProjectItemModel::getRootFolder() const
{
    return static_cast<ProjectFolder*>(rootItem);
}

Bin *ProjectItemModel::bin() const
{
    return m_bin;
}
