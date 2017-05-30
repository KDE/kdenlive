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
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "projectclip.h"
#include "projectfolder.h"
#include "projectsubclip.h"
#include "profiles/profilemodel.hpp"

#include <KLocalizedString>
#include <QIcon>
#include <QMimeData>
#include <qvarlengtharray.h>
#include <mlt++/Mlt.h>

ProjectItemModel::ProjectItemModel(Bin *bin, QObject *parent)
    : AbstractTreeModel(parent)
    , m_lock(QReadWriteLock::Recursive)
    , m_binPlaylist(new Mlt::Playlist(pCore->getCurrentProfile()->profile()))
    , m_bin(bin)
{
}

std::shared_ptr<ProjectItemModel> ProjectItemModel::construct(Bin *bin, QObject *parent)
{
    std::shared_ptr<ProjectItemModel> self(new ProjectItemModel(bin, parent));
    self->rootItem = ProjectFolder::construct(self);
    return self;
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
    READ_LOCK();
    if (!index.isValid()) {
        return QVariant();
    }
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(index);
        auto type = static_cast<AbstractProjectItem::DataType>(mapToColumn(index.column()));
        QVariant ret = item->getData(type);
        return ret;
    }
    if (role == Qt::DecorationRole) {
        if (index.column() != 0) {
            return QVariant();
        }
        // Data has to be returned as icon to allow the view to scale it
        std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(index);
        QVariant thumb = item->getData(AbstractProjectItem::DataThumbnail);
        QIcon icon;
        if (thumb.canConvert<QIcon>()) {
            icon = thumb.value<QIcon>();
        } else {
            qDebug() << "ERROR: invalid icon found";
        }
        return icon;
    }
    std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(index);
    return item->getData((AbstractProjectItem::DataType)role);
}

bool ProjectItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(index);
    if (item->rename(value.toString(), index.column())) {
        emit dataChanged(index, index, QVector<int>() << role);
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
    std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(index);
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
        return getBinItemByIndex(parent)->supportedDataCount();
    }
    return std::static_pointer_cast<ProjectFolder>(rootItem)->supportedDataCount();
}

// cppcheck-suppress unusedFunction
Qt::DropActions ProjectItemModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

QStringList ProjectItemModel::mimeTypes() const
{
    QStringList types;
    types << QStringLiteral("kdenlive/producerslist") << QStringLiteral("text/uri-list") << QStringLiteral("kdenlive/clip")
          << QStringLiteral("kdenlive/effectslist");
    return types;
}

QMimeData *ProjectItemModel::mimeData(const QModelIndexList &indices) const
{
    // Mime data is a list of id's separated by ';'.
    // Clip ids are represented like:  2 (where 2 is the clip's id)
    // Clip zone ids are represented like:  2/10/200 (where 2 is the clip's id, 10 and 200 are in and out points)
    // Folder ids are represented like:  #2 (where 2 is the folder's id)
    auto *mimeData = new QMimeData();
    QStringList list;
    int duration = 0;
    for (int i = 0; i < indices.count(); i++) {
        QModelIndex ix = indices.at(i);
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(ix);
        AbstractProjectItem::PROJECTITEMTYPE type = item->itemType();
        if (type == AbstractProjectItem::ClipItem) {
            list << item->clipId();
            duration += (std::static_pointer_cast<ProjectClip>(item))->frameDuration();
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
        mimeData->setData(QStringLiteral("kdenlive/producerslist"), data);
        qDebug() << "/// CLI DURATION: " << duration;
        mimeData->setText(QString::number(duration));
    }
    return mimeData;
}

void ProjectItemModel::onItemUpdated(std::shared_ptr<AbstractProjectItem> item)
{
    auto tItem = std::static_pointer_cast<TreeItem>(item);
    auto ptr = tItem->parentItem().lock();
    if (ptr) {
        auto index = getIndexFromItem(tItem);
        emit dataChanged(index, index);
    }
}

std::shared_ptr<ProjectClip> ProjectItemModel::getClipByBinID(const QString &binId)
{
    if (binId.contains(QLatin1Char('_'))) {
        return getClipByBinID(binId.section(QLatin1Char('_'), 0, 0));
    }
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second);
        if (c->itemType() == AbstractProjectItem::ClipItem && c->clipId() == binId) {
            return std::static_pointer_cast<ProjectClip>(c);
        }
    }
    return nullptr;
}

std::shared_ptr<ProjectFolder> ProjectItemModel::getFolderByBinId(const QString &binId)
{
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second);
        if (c->itemType() == AbstractProjectItem::FolderItem && c->clipId() == binId) {
            return std::static_pointer_cast<ProjectFolder>(c);
        }
    }
    return nullptr;
}

void ProjectItemModel::setBinEffectsEnabled(bool enabled)
{
    return std::static_pointer_cast<AbstractProjectItem>(rootItem)->setBinEffectsEnabled(enabled);
}

QStringList ProjectItemModel::getEnclosingFolderInfo(const QModelIndex &index) const
{
    QStringList noInfo;
    noInfo << QString::number(-1);
    noInfo << QString();
    if (!index.isValid()) {
        return noInfo;
    }

    std::shared_ptr<AbstractProjectItem> currentItem = getBinItemByIndex(index);
    auto folder = currentItem->getEnclosingFolder(true);
    if ((folder == nullptr) || folder == rootItem) {
        return noInfo;
    }
    QStringList folderInfo;
    folderInfo << currentItem->clipId();
    folderInfo << currentItem->name();
    return folderInfo;
}

void ProjectItemModel::clean()
{
    rootItem = ProjectFolder::construct(std::static_pointer_cast<ProjectItemModel>(shared_from_this()));
}

std::shared_ptr<ProjectFolder> ProjectItemModel::getRootFolder() const
{
    return std::static_pointer_cast<ProjectFolder>(rootItem);
}

void ProjectItemModel::loadSubClips(const QString &id, const QMap<QString, QString> &dataMap)
{
    std::shared_ptr<ProjectClip> clip = getClipByBinID(id);
    if (!clip) {
        return;
    }
    QMapIterator<QString, QString> i(dataMap);
    QList<int> missingThumbs;
    int maxFrame = clip->duration().frames(pCore->getCurrentFps()) - 1;
    while (i.hasNext()) {
        i.next();
        if (!i.value().contains(QLatin1Char(';'))) {
            // Problem, the zone has no in/out points
            continue;
        }
        int in = i.value().section(QLatin1Char(';'), 0, 0).toInt();
        int out = i.value().section(QLatin1Char(';'), 1, 1).toInt();
        if (maxFrame > 0) {
            out = qMin(out, maxFrame);
        }
        missingThumbs << in;
        // TODO remove access to doc here
        auto self = std::static_pointer_cast<ProjectItemModel>(shared_from_this());
        ProjectSubClip::construct(clip, self, in, out, pCore->currentDoc()->timecode().getDisplayTimecodeFromFrames(in, KdenliveSettings::frametimecode()),
                                  i.key());
    }
    if (!missingThumbs.isEmpty()) {
        // generate missing subclip thumbnails
        clip->slotExtractImage(missingThumbs);
    }
}

Bin *ProjectItemModel::bin() const
{
    return m_bin;
}

std::shared_ptr<AbstractProjectItem> ProjectItemModel::getBinItemByIndex(const QModelIndex &index) const
{
    return std::static_pointer_cast<AbstractProjectItem>(getItemById((int)index.internalId()));
}

bool ProjectItemModel::requestBinClipDeletion(std::shared_ptr<AbstractProjectItem> clip, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(clip);
    if (!clip) return false;
    int parentId = -1;
    if (auto ptr = clip->parent()) parentId = ptr->getId();
    int id = clip->getId();
    Fun operation = [this, id]() {
        /* Deletion simply deregister clip and remove it from parent.
           The actual object is not actually deleted, because a shared_pointer to it
           is captured by the reverse operation.
           Actual deletions occurs when the undo object is destroyed.
        */
        auto currentClip = std::static_pointer_cast<AbstractProjectItem>(m_allItems[id]);
        Q_ASSERT(currentClip);
        if (!currentClip) return false;
        auto parent = currentClip->parent();
        parent->removeChild(currentClip);
        return true;
    };
    Fun reverse = [this, clip, parentId]() {
        /* To undo deletion, we reregister the clip */
        std::shared_ptr<TreeItem> parent;
        qDebug() << "reversing deletion. Parentid =" << parentId;
        if (parentId != -1) {
            qDebug() << "parent found";
            parent = getItemById(parentId);
        }
        clip->changeParent(parent);
        return true;
    };
    bool res = operation();
    qDebug() << "operation succes" << res;
    if (res) {
        LOCK_IN_LAMBDA(operation);
        LOCK_IN_LAMBDA(reverse);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        res = clip->selfSoftDelete(undo, redo);
        qDebug() << "soltDelete succes" << res;
    }
    if (res) {
    }
    qDebug() << "Deletion success" << res;
    return res;
}

void ProjectItemModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    auto clip = std::static_pointer_cast<AbstractProjectItem>(item);
    manageBinClipInsertion(clip);
    AbstractTreeModel::registerItem(item);
}
void ProjectItemModel::deregisterItem(int id)
{
    auto clip = std::static_pointer_cast<AbstractProjectItem>(m_allItems[id]);
    if (auto parent = clip->parent()) {
        manageBinClipDeletion(clip, parent);
    }
    AbstractTreeModel::deregisterItem(id);
}

void ProjectItemModel::notifyRowAppended(const std::shared_ptr<TreeItem> &row)
{
    auto binElem = std::static_pointer_cast<AbstractProjectItem>(row);
    manageBinClipInsertion(binElem);
    AbstractTreeModel::notifyRowAppended(row);
}

void ProjectItemModel::manageBinClipInsertion(const std::shared_ptr<AbstractProjectItem> &binElem)
{
    switch(binElem->itemType()) {
    case AbstractProjectItem::FolderItem: {
        //When a folder is inserted, we have to store its path into the properties
        if (binElem->parent()) {
            QString propertyName = "kdenlive:folder." + binElem->parent()->clipId() + QLatin1Char('.') + binElem->clipId();
            m_binPlaylist->set(propertyName.toUtf8().constData(), binElem->name().toUtf8().constData());
        }
        break;
    }
    default:
        break;
    }
}

void ProjectItemModel::notifyRowAboutToDelete(std::shared_ptr<TreeItem> item, int row)
{
    auto rowItem = item->child(row);
    auto binElem = std::static_pointer_cast<AbstractProjectItem>(rowItem);
    auto oldParent = std::static_pointer_cast<AbstractProjectItem>(item);
    manageBinClipDeletion(binElem, oldParent);
    AbstractTreeModel::notifyRowAboutToDelete(item, row);
}

void ProjectItemModel::manageBinClipDeletion(std::shared_ptr<AbstractProjectItem> binElem, std::shared_ptr<AbstractProjectItem> oldParent)
{
    switch(binElem->itemType()) {
    case AbstractProjectItem::FolderItem: {
        //When a folder is removed, we clear the path info
        QString propertyName = "kdenlive:folder." + oldParent->clipId() + QLatin1Char('.') + binElem->clipId();
        m_binPlaylist->set(propertyName.toUtf8().constData(), (char *)nullptr);
        break;
    }
    default:
        break;
    }
}

// bool ProjectItemModel::requestAddFolder(const QString &name, const QString &parentId, Fun &undo, Fun &redo)
// {
//     QWriteLocker locker(&m_lock);
//     std::shared_ptr<ProjectFolder> parentFolder = getFolderByBinId(parentId);
//     if (!parentFolder) {
//         qCDebug(KDENLIVE_LOG) << "  / / ERROR IN PARENT FOLDER";
//         return false;
//     }
//     std::shared_ptr<ProjectFolder> new_folder = ProjectFolder::construct(id, name, m_itemModel, parentFolder);
//     int folderId = new_folder->getId();
//     Fun operation = [this, new_folder, parentId]() {
//         /* Insertion is simply setting the parent of the folder.*/
//         std::shared_ptr<ProjectFolder> parentFolder = getFolderByBinId(parentId);
//         if (!parentFolder) {
//             return false;
//         }
//         new_folder->changeParent(parentFolder);
//         return true;
//     };
//     Fun reverse = [this, folderId]() {
//         /* To undo insertion, we deregister the clip */
//         std::shared_ptr<AbstractProjectItem> folder = getClipByBinID(folderId);
//         if (!folder) {
//             return false;
//         }
//         auto parent = currentClip->parent();
//         parent->removeChild(currentClip);
//         return true;
//     };
//     bool res = operation();
//     if (res) {
//         LOCK_IN_LAMBDA(operation);
//         LOCK_IN_LAMBDA(reverse);
//         UPDATE_UNDO_REDO(operation, reverse, undo, redo);
//     }
//     return res;
// }
