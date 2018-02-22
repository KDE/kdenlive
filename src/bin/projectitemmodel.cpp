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
#include "jobs/audiothumbjob.hpp"
#include "jobs/jobmanager.h"
#include "jobs/loadjob.hpp"
#include "jobs/thumbjob.hpp"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
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
        if (ids.constFirst().contains(QLatin1Char('/'))) {
            // subclip zone
            QStringList clipData = ids.constFirst().split(QLatin1Char('/'));
            if (clipData.length() >= 3) {
                QString id;
                return requestAddBinSubClip(id, clipData.at(1).toInt(), clipData.at(2).toInt(), clipData.at(0));
            } else {
                // error, malformed clip zone, abort
                return false;
            }
        } else {
            emit itemDropped(ids, parent);
        }
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
        QString id;
        return requestAddBinSubClip(id, list.at(1).toInt(), list.at(2).toInt(), list.at(0));
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
            list << std::static_pointer_cast<ProjectSubClip>(item)->getMasterClip()->clipId() + QLatin1Char('/') + QString::number(p.x()) + QLatin1Char('/') + QString::number(p.y());
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

void ProjectItemModel::onItemUpdated(std::shared_ptr<AbstractProjectItem> item, int role)
{
    auto tItem = std::static_pointer_cast<TreeItem>(item);
    auto ptr = tItem->parentItem().lock();
    if (ptr) {
        auto index = getIndexFromItem(tItem);
        emit dataChanged(index, index, QVector<int>() << role);
    }
}

void ProjectItemModel::onItemUpdated(const QString &binId, int role)
{
    std::shared_ptr<AbstractProjectItem> item = getItemByBinId(binId);
    if (item) {
        onItemUpdated(item, role);
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

bool ProjectItemModel::hasClip(const QString &binId)
{
    return getClipByBinID(binId) != nullptr;
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

std::shared_ptr<AbstractProjectItem> ProjectItemModel::getItemByBinId(const QString &binId)
{
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->clipId() == binId) {
            return c;
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
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
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

        QString subId;
        requestAddBinSubClip(subId, in, out, id, undo, redo);
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
    // TODO : here, we should suspend jobs belonging to the item we delete. They can be restarted if the item is reinserted by undo
    AbstractTreeModel::deregisterItem(id, item);
}

int ProjectItemModel::getFreeFolderId()
{
    while (!isIdFree(QString::number(++m_nextId))) {
    };
    return m_nextId;
}

int ProjectItemModel::getFreeClipId()
{
    while (!isIdFree(QString::number(++m_nextId))) {
    };
    return m_nextId;
}

bool ProjectItemModel::addItem(std::shared_ptr<AbstractProjectItem> item, const QString &parentId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<AbstractProjectItem> parentItem = getItemByBinId(parentId);
    if (!parentItem) {
        qCDebug(KDENLIVE_LOG) << "  / / ERROR IN PARENT FOLDER";
        return false;
    }
    if (item->itemType() == AbstractProjectItem::ClipItem && parentItem->itemType() != AbstractProjectItem::FolderItem) {
        qCDebug(KDENLIVE_LOG) << "  / / ERROR when inserting clip: a clip should be inserted in a folder";
        return false;
    }
    if (item->itemType() == AbstractProjectItem::SubClipItem && parentItem->itemType() != AbstractProjectItem::ClipItem) {
        qCDebug(KDENLIVE_LOG) << "  / / ERROR when inserting subclip: a subclip should be inserted in a clip";
        return false;
    }
    Fun operation = addItem_lambda(item, parentItem->getId());

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
    qDebug() << "/////////// requestAddBinClip" << parentId;
    QWriteLocker locker(&m_lock);
    if (id.isEmpty()) {
        id =
            Xml::getTagContentByAttribute(description, QStringLiteral("property"), QStringLiteral("name"), QStringLiteral("kdenlive:id"), QStringLiteral("-1"));
        if (id == QStringLiteral("-1") || !isIdFree(id)) {
            id = QString::number(getFreeClipId());
        }
    }
    Q_ASSERT(!id.isEmpty() && isIdFree(id));
    qDebug() << "/////////// found id" << id;
    std::shared_ptr<ProjectClip> new_clip =
        ProjectClip::construct(id, description, m_blankThumb, std::static_pointer_cast<ProjectItemModel>(shared_from_this()));
    qDebug() << "/////////// constructed ";
    bool res = addItem(new_clip, parentId, undo, redo);
    qDebug() << "/////////// added " << res;
    if (res) {
        int loadJob = pCore->jobManager()->startJob<LoadJob>({id}, -1, QString(), description);
        pCore->jobManager()->startJob<ThumbJob>({id}, loadJob, QString(), 150, 0, true);
        pCore->jobManager()->startJob<AudioThumbJob>({id}, loadJob, QString());
    }
    return res;
}

bool ProjectItemModel::requestAddBinClip(QString &id, const QDomElement &description, const QString &parentId, const QString &undoText)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestAddBinClip(id, description, parentId, undo, redo);
    if (res) {
        pCore->pushUndo(undo, redo, undoText.isEmpty() ? i18n("Add bin clip") : undoText);
    }
    return res;
}

bool ProjectItemModel::requestAddBinClip(QString &id, std::shared_ptr<Mlt::Producer> producer, const QString &parentId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (id.isEmpty()) {
        id = QString::number(producer->get_int("kdenlive:id"));
        if (!isIdFree(id)) {
            id = QString::number(getFreeClipId());
        }
    }
    Q_ASSERT(!id.isEmpty() && isIdFree(id));
    std::shared_ptr<ProjectClip> new_clip = ProjectClip::construct(id, m_blankThumb, std::static_pointer_cast<ProjectItemModel>(shared_from_this()), producer);
    bool res = addItem(new_clip, parentId, undo, redo);
    if (res) {
        int blocking = pCore->jobManager()->getBlockingJobId(id, AbstractClipJob::LOADJOB);
        pCore->jobManager()->startJob<ThumbJob>({id}, blocking, QString(), 150, -1, true);
        pCore->jobManager()->startJob<AudioThumbJob>({id}, blocking, QString());
    }
    return res;
}

bool ProjectItemModel::requestAddBinSubClip(QString &id, int in, int out, const QString &parentId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (id.isEmpty()) {
        id = QString::number(getFreeClipId());
    }
    Q_ASSERT(!id.isEmpty() && isIdFree(id));
    auto clip = getClipByBinID(parentId);
    Q_ASSERT(clip->itemType() == AbstractProjectItem::ClipItem);
    auto tc = pCore->currentDoc()->timecode().getDisplayTimecodeFromFrames(in, KdenliveSettings::frametimecode());
    std::shared_ptr<ProjectSubClip> new_clip = ProjectSubClip::construct(id, clip, std::static_pointer_cast<ProjectItemModel>(shared_from_this()), in, out, tc);
    bool res = addItem(new_clip, parentId, undo, redo);
    if (res) {
        int parentJob = pCore->jobManager()->getBlockingJobId(parentId, AbstractClipJob::LOADJOB);
        pCore->jobManager()->startJob<ThumbJob>({id}, parentJob, QString(), 150, -1, true);
    }
    return res;
}
bool ProjectItemModel::requestAddBinSubClip(QString &id, int in, int out, const QString &parentId)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestAddBinSubClip(id, in, out, parentId, undo, redo);
    if (res) {
        pCore->pushUndo(undo, redo, i18n("Add a sub clip"));
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

bool ProjectItemModel::loadFolders(Mlt::Properties &folders)
{
    // At this point, we expect the folders properties to have a name of the form "x.y" where x is the id of the parent folder and y the id of the child.
    // Note that for root folder, x = -1
    // The value of the property is the name of the child folder

    std::unordered_map<int, std::vector<int>> downLinks; // key are parents, value are children
    std::unordered_map<int, int> upLinks;                // key are children, value are parent
    std::unordered_map<int, QString> newIds;             // we store the correspondance to the new ids
    std::unordered_map<int, QString> folderNames;
    newIds[-1] = getRootFolder()->clipId();

    if (folders.count() == 0) return true;

    for (int i = 0; i < folders.count(); i++) {
        QString folderName = folders.get(i);
        QString id = folders.get_name(i);

        int parentId = id.section(QLatin1Char('.'), 0, 0).toInt();
        int folderId = id.section(QLatin1Char('.'), 1, 1).toInt();
        downLinks[parentId].push_back(folderId);
        upLinks[folderId] = parentId;
        folderNames[folderId] = folderName;
        qDebug() << "Found folder " << folderId << "name = " << folderName << "parent=" << parentId;
    }

    // In case there are some non-existant parent, we fall back to root
    for (const auto &f : downLinks) {
        if (upLinks.count(f.first) == 0) {
            upLinks[f.first] = -1;
        }
        if (f.first != -1 && downLinks.count(upLinks[f.first]) == 0) {
            qDebug() << "Warning: parent folder " << upLinks[f.first] << "for folder" << f.first << "is invalid. Folder will be placed in topmost directory.";
            upLinks[f.first] = -1;
        }
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

bool ProjectItemModel::isIdFree(const QString &id) const
{
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->clipId() == id) {
            return false;
        }
    }
    return true;
}

void ProjectItemModel::loadBinPlaylist(Mlt::Tractor *documentTractor, Mlt::Tractor *modelTractor, std::unordered_map<QString, QString> &binIdCorresp)
{
    clean();
    Mlt::Properties retainList((mlt_properties)documentTractor->get_data("xml_retain"));
    qDebug() << "Loading bin playlist...";
    if (retainList.is_valid() && (retainList.get_data(BinPlaylist::binPlaylistId.toUtf8().constData()) != nullptr)) {
        Mlt::Playlist playlist((mlt_playlist)retainList.get_data(BinPlaylist::binPlaylistId.toUtf8().constData()));
        qDebug() << "retain is valid";
        if (playlist.is_valid() && playlist.type() == playlist_type) {
            qDebug() << "playlist is valid";

            // Load bin clips
            qDebug() << "init bin";
            // Load folders
            Mlt::Properties folderProperties;
            Mlt::Properties playlistProps(playlist.get_properties());
            folderProperties.pass_values(playlistProps, "kdenlive:folder.");
            loadFolders(folderProperties);

            // Read notes
            QString notes = playlistProps.get("kdenlive:documentnotes");
            pCore->projectManager()->setDocumentNotes(notes);

            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            qDebug() << "Found " << playlist.count() << "clips";
            int max = playlist.count();
            for (int i = 0; i < max; i++) {
                QScopedPointer<Mlt::Producer> prod(playlist.get_clip(i));
                std::shared_ptr<Mlt::Producer> producer(new Mlt::Producer(prod->parent()));
                qDebug() << "dealing with bin clip" << i;
                if (producer->is_blank() || !producer->is_valid()) {
                    qDebug() << "producer is not valid or blank";
                    continue;
                }
                QString id = qstrdup(producer->get("kdenlive:id"));
                QString parentId = qstrdup(producer->get("kdenlive:folderid"));
                qDebug() << "clip id" << id;
                if (id.contains(QLatin1Char('_'))) {
                    // TODO refac ?
                    /*
                    // This is a track producer
                    QString mainId = id.section(QLatin1Char('_'), 0, 0);
                    // QString track = id.section(QStringLiteral("_"), 1, 1);
                    if (m_clipList.contains(mainId)) {
                        // The controller for this track producer already exists
                    } else {
                        // Create empty controller for this clip
                        requestClipInfo info;
                        info.imageHeight = 0;
                        info.clipId = id;
                        info.replaceProducer = true;
                        emit slotProducerReady(info, ClipController::mediaUnavailable);
                    }
                    */
                } else {
                    QString newId = isIdFree(id) ? id : QString::number(getFreeClipId());
                    producer->set("_kdenlive_processed", 1);
                    requestAddBinClip(newId, producer, parentId, undo, redo);
                    binIdCorresp[id] = newId;
                    qDebug() << "Loaded clip "<< id <<"under id"<<newId;
                }
            }
        }
    }
    m_binPlaylist->setRetainIn(modelTractor);
}

/** @brief Save document properties in MLT's bin playlist */
void ProjectItemModel::saveDocumentProperties(const QMap<QString, QString> &props, const QMap<QString, QString> &metadata,
                                              std::shared_ptr<MarkerListModel> guideModel)
{
    m_binPlaylist->saveDocumentProperties(props, metadata, guideModel);
}

void ProjectItemModel::saveProperty(const QString &name, const QString &value)
{
    m_binPlaylist->saveProperty(name, value);
}
