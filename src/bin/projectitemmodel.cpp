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
#include "binplaylist.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "profiles/profilemodel.hpp"
#include "projectclip.h"
#include "projectfolder.h"
#include "projectsubclip.h"
#include "xml/xml.hpp"

#include <KLocalizedString>
#include <QIcon>
#include <QMimeData>
#include <mlt++/Mlt.h>
#include <queue>
#include <qvarlengtharray.h>

ProjectItemModel::ProjectItemModel(QObject *parent)
    : AbstractTreeModel(parent)
    , m_lock(QReadWriteLock::Recursive)
    , m_binPlaylist(new BinPlaylist())
    , m_nextId(1)
    , m_blankThumb()
{
    KImageCache::deleteCache(QStringLiteral("kdenlive-thumbs"));
    m_pixmapCache.reset(new KImageCache(QStringLiteral("kdenlive-thumbs"), 10000000));
    m_pixmapCache->setEvictionPolicy(KSharedDataCache::EvictOldest);

    QPixmap pix(QSize(160, 90));
    pix.fill(Qt::lightGray);
    m_blankThumb.addPixmap(pix);

}

std::shared_ptr<ProjectItemModel> ProjectItemModel::construct(QObject *parent)
{
    std::shared_ptr<ProjectItemModel> self(new ProjectItemModel(parent));
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
        const QStringList ids = QString(data->data(QStringLiteral("kdenlive/producerslist"))).split(QLatin1Char(';'));
        emit itemDropped(ids, parent);
        return true;
    }

    if (data->hasFormat(QStringLiteral("kdenlive/effect"))) {
        // Dropping effect on a Bin item
        QStringList effectData;
        effectData << QString::fromUtf8(data->data(QStringLiteral("kdenlive/effect")));
        QStringList source = QString::fromUtf8(data->data(QStringLiteral("kdenlive/effectsource"))).split(QLatin1Char('-'));
        effectData << source;
        emit effectDropped(effectData, parent);
        return true;
    }

    if (data->hasFormat(QStringLiteral("kdenlive/clip"))) {
        const QStringList list = QString(data->data(QStringLiteral("kdenlive/clip"))).split(QLatin1Char(';'));
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
          << QStringLiteral("kdenlive/effect");
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
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::ClipItem && c->clipId() == binId) {
            return std::static_pointer_cast<ProjectClip>(c);
        }
    }
    return nullptr;
}

std::shared_ptr<ProjectFolder> ProjectItemModel::getFolderByBinId(const QString &binId)
{
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
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
    std::vector<std::shared_ptr<AbstractProjectItem>> toDelete;
    for (int i = 0; i < rootItem->childCount(); ++i) {
        toDelete.push_back(std::static_pointer_cast<AbstractProjectItem>(rootItem->child(i)));
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    for (const auto &child : toDelete) {
        requestBinClipDeletion(child, undo, redo);
    }
    Q_ASSERT(rootItem->childCount() == 0);
    m_nextId = 1;
    m_pixmapCache->clear();
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
    clip->selfSoftDelete(undo, redo);
    int id = clip->getId();
    Fun operation = removeItem_lambda(id);
    Fun reverse = addItem_lambda(clip, parentId);
    bool res = operation();
    if (res) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    }
    return res;
}

void ProjectItemModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    auto clip = std::static_pointer_cast<AbstractProjectItem>(item);
    m_binPlaylist->manageBinItemInsertion(clip);
    AbstractTreeModel::registerItem(item);
}
void ProjectItemModel::deregisterItem(int id, TreeItem *item)
{
    auto clip = static_cast<AbstractProjectItem *>(item);
    m_binPlaylist->manageBinItemDeletion(clip);
    AbstractTreeModel::deregisterItem(id, item);
}


int ProjectItemModel::getFreeFolderId()
{
    return m_nextId++;
}

int ProjectItemModel::getFreeClipId()
{
    return m_nextId++;
}

bool ProjectItemModel::addItem(std::shared_ptr<AbstractProjectItem> item, const QString &parentId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<ProjectFolder> parentFolder = getFolderByBinId(parentId);
    if (!parentFolder) {
        qCDebug(KDENLIVE_LOG) << "  / / ERROR IN PARENT FOLDER";
        return false;
    }
    Fun operation = addItem_lambda(item, parentFolder->getId());
    int itemId = item->getId();
    Fun reverse = removeItem_lambda(itemId);
    bool res = operation();
    Q_ASSERT(item->isInModel());
    if (res) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    }
    return res;
}

bool ProjectItemModel::requestAddFolder(QString &id, const QString &name, const QString &parentId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (!id.isEmpty() && !isIdFree(id)) {
        id = QString();
    }
    if (id.isEmpty()) {
        id = QString::number(getFreeFolderId());
    }
    std::shared_ptr<ProjectFolder> new_folder = ProjectFolder::construct(id, name, std::static_pointer_cast<ProjectItemModel>(shared_from_this()));
    return addItem(new_folder, parentId, undo, redo);
}

bool ProjectItemModel::requestAddBinClip(QString &id, const QDomElement &description, const QString &parentId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (id.isEmpty()) {
        QString defaultId = QString::number(getFreeClipId());
        id = Xml::getTagContentByAttribute(description, QStringLiteral("property"), QStringLiteral("name"), QStringLiteral("kdenlive:id"), defaultId);
    }
    std::shared_ptr<ProjectClip> new_clip = ProjectClip::construct(id, description, m_blankThumb, std::static_pointer_cast<ProjectItemModel>(shared_from_this()));
    bool res = addItem(new_clip, parentId, undo, redo);
    if (res) {
        //TODO Not very clean, improve (should pass pointer to projectClip instead of xml)
        pCore->currentDoc()->getFileProperties(description, new_clip->clipId(), 150, true);
    }
    return res;
}

bool ProjectItemModel::requestAddBinClip(QString &id, const QDomElement &description, const QString &parentId, const QString &undoText)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestAddBinClip(id, description, parentId, undo, redo);
    if (res) {
        pCore->pushUndo(undo, redo, undoText.isEmpty() ? i18n("Rename Folder") : undoText);
    }
    return res;
}

Fun ProjectItemModel::requestRenameFolder_lambda(std::shared_ptr<AbstractProjectItem> folder, const QString &newName)
{
    int id = folder->getId();
    return [this, id, newName]() {
        auto currentFolder = std::static_pointer_cast<AbstractProjectItem>(m_allItems[id].lock());
        if (!currentFolder) {
            return false;
        }
        // For correct propagation of the name change, we remove folder from parent first
        auto parent = currentFolder->parent();
        parent->removeChild(currentFolder);
        currentFolder->setName(newName);
        // Reinsert in parent
        return parent->appendChild(currentFolder);
    };
}

bool ProjectItemModel::requestRenameFolder(std::shared_ptr<AbstractProjectItem> folder, const QString &name, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    QString oldName = folder->name();
    auto operation = requestRenameFolder_lambda(folder, name);
    if (operation()) {
        auto reverse = requestRenameFolder_lambda(folder, oldName);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

bool ProjectItemModel::requestRenameFolder(std::shared_ptr<AbstractProjectItem> folder, const QString &name)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestRenameFolder(folder, name, undo, redo);
    if (res) {
        pCore->pushUndo(undo, redo, i18n("Rename Folder"));
    }
    return res;
}


bool ProjectItemModel::requestCleanup()
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = true;
    std::vector<std::shared_ptr<AbstractProjectItem>> to_delete;
    // Iterate to find clips that are not in timeline
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::ClipItem && !c->isIncludedInTimeline()) {
            to_delete.push_back(c);
        }
    }
    // it is important to execute deletion in a separate loop, because otherwise
    // the iterators of m_allItems get messed up
    for (const auto &c : to_delete) {
        res = requestBinClipDeletion(c, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    pCore->pushUndo(undo, redo, i18n("Clean Project"));
    return true;
}

std::vector<QString> ProjectItemModel::getAllClipIds() const
{
    std::vector<QString> result;
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::ClipItem) {
            result.push_back(c->clipId());
        }
    }
    return result;
}


bool ProjectItemModel::loadFolders(Mlt::Properties& folders)
{
    // At this point, we expect the folders properties to have a name of the form "x.y" where x is the id of the parent folder and y the id of the child.
    // Note that for root folder, x = -1
    // The value of the property is the name of the child folder

    std::unordered_map<int, std::vector<int>> downLinks; //key are parents, value are children
    std::unordered_map<int, int> upLinks; //key are children, value are parent
    std::unordered_map<int, QString> newIds; // we store the correspondance to the new ids
    std::unordered_map<int, QString> folderNames;
    newIds[-1] = getRootFolder()->clipId();

    if (folders.count() == 0) return true;

    qDebug() << "found "<<folders.count()<<"folders";
    for (int i = 0; i < folders.count(); i++) {
        QString folderName = folders.get(i);
        QString id = folders.get_name(i);

        int parentId = id.section(QLatin1Char('.'), 0, 0).toInt();
        int folderId = id.section(QLatin1Char('.'), 1, 1).toInt();
        qDebug() << folderName<<id<<parentId<<folderId;
        downLinks[parentId].push_back(folderId);
        upLinks[folderId] = parentId;
        folderNames[folderId] = folderName;
    }

    // We now do a BFS to construct the folders in order
    Q_ASSERT(downLinks.count(-1) > 0);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::queue<int> queue;
    std::unordered_set<int> seen;
    queue.push(-1);
    while (!queue.empty()) {
        int current = queue.front();
        seen.insert(current);
        queue.pop();
        if (current != -1) {
            QString id = QString::number(current);
            bool res = requestAddFolder(id, folderNames[current], newIds[upLinks[current]], undo, redo);
            if (!res) {
                bool undone = undo();
                Q_ASSERT(undone);
                return false;
            }
            newIds[current] = id;
        }
        for (int c : downLinks[current]) {
            queue.push(c);
        }
    }
    return true;
}


bool ProjectItemModel::isIdFree(const QString& id) const
{
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->clipId() == id) {
            return false;
        }
    }
    return true;
}
