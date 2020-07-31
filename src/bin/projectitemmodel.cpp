/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2017  Nicolas Carion
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
#include "filewatcher.hpp"
#include "jobs/audiothumbjob.hpp"
#include "jobs/jobmanager.h"
#include "jobs/loadjob.hpp"
#include "jobs/thumbjob.hpp"
#include "jobs/cachejob.hpp"
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
#include <QProgressDialog>
#include <mlt++/Mlt.h>
#include <queue>
#include <qvarlengtharray.h>
#include <utility>

ProjectItemModel::ProjectItemModel(QObject *parent)
    : AbstractTreeModel(parent)
    , m_lock(QReadWriteLock::Recursive)
    , m_binPlaylist(new BinPlaylist())
    , m_fileWatcher(new FileWatcher())
    , m_nextId(1)
    , m_blankThumb()
    , m_dragType(PlaylistState::Disabled)
{
    QPixmap pix(QSize(160, 90));
    pix.fill(Qt::lightGray);
    m_blankThumb.addPixmap(pix);
    connect(m_fileWatcher.get(), &FileWatcher::binClipModified, this, &ProjectItemModel::reloadClip);
    connect(m_fileWatcher.get(), &FileWatcher::binClipWaiting, this, &ProjectItemModel::setClipWaiting);
    connect(m_fileWatcher.get(), &FileWatcher::binClipMissing, this, &ProjectItemModel::setClipInvalid);
}

std::shared_ptr<ProjectItemModel> ProjectItemModel::construct(QObject *parent)
{
    std::shared_ptr<ProjectItemModel> self(new ProjectItemModel(parent));
    self->rootItem = ProjectFolder::construct(self);
    return self;
}

ProjectItemModel::~ProjectItemModel() = default;

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
    case 3:
        return AbstractProjectItem::ClipType;
        break;
    case 4:
        return AbstractProjectItem::DataTag;
        break;
    case 5:
        return AbstractProjectItem::DataDuration;
        break;
    case 6:
        return AbstractProjectItem::DataId;
        break;
    case 7:
        return AbstractProjectItem::DataRating;
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
    return item->getData(static_cast<AbstractProjectItem::DataType>(role));
}

bool ProjectItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(index);
    if (item->rename(value.toString(), index.column())) {
        emit dataChanged(index, index, {role});
        return true;
    }
    // Item name was not changed
    return false;
}

Qt::ItemFlags ProjectItemModel::flags(const QModelIndex &index) const
{
    /*return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;*/
    READ_LOCK();
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
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        }
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEditable;
        break;
    case AbstractProjectItem::SubClipItem:
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
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
    QWriteLocker locker(&m_lock);
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
                std::shared_ptr<ProjectClip> masterClip = getClipByBinID(clipData.at(0));
                std::shared_ptr<ProjectSubClip> sub = masterClip->getSubClip(clipData.at(1).toInt(), clipData.at(2).toInt());
                if (sub != nullptr) {
                    // This zone already exists
                    return false;
                }
                return requestAddBinSubClip(id, clipData.at(1).toInt(), clipData.at(2).toInt(), {}, clipData.at(0));
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
        return requestAddBinSubClip(id, list.at(1).toInt(), list.at(2).toInt(), {}, list.at(0));
    }

    if (data->hasFormat(QStringLiteral("kdenlive/tag"))) {
        // Dropping effect on a Bin item
        QString tag = QString::fromUtf8(data->data(QStringLiteral("kdenlive/tag")));
        emit addTag(tag, parent);
        return true;
    }

    return false;
}

QVariant ProjectItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    READ_LOCK();
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
        case 3:
            columnName = i18n("Type");
            break;
        case 4:
            columnName = i18n("Tag");
            break;
        case 5:
            columnName = i18n("Duration");
            break;
        case 6:
            columnName = i18n("Id");
            break;
        case 7:
            columnName = i18n("Rating");
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
    READ_LOCK();
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
    QStringList types {QStringLiteral("kdenlive/producerslist"), QStringLiteral("text/uri-list"), QStringLiteral("kdenlive/clip"), QStringLiteral("kdenlive/effect"), QStringLiteral("kdenlive/tag")};
    return types;
}

QMimeData *ProjectItemModel::mimeData(const QModelIndexList &indices) const
{
    READ_LOCK();
    // Mime data is a list of id's separated by ';'.
    // Clip ids are represented like:  2 (where 2 is the clip's id)
    // Clip zone ids are represented like:  2/10/200 (where 2 is the clip's id, 10 and 200 are in and out points)
    // Folder ids are represented like:  #2 (where 2 is the folder's id)
    auto *mimeData = new QMimeData();
    QStringList list;
    size_t duration = 0;
    for (int i = 0; i < indices.count(); i++) {
        QModelIndex ix = indices.at(i);
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(ix);
        AbstractProjectItem::PROJECTITEMTYPE type = item->itemType();
        if (type == AbstractProjectItem::ClipItem) {
            ClipType::ProducerType cType = item->clipType();
            QString dragId = item->clipId();
            if ((cType == ClipType::AV || cType == ClipType::Playlist)) {
                switch (m_dragType) {
                case PlaylistState::AudioOnly:
                    dragId.prepend(QLatin1Char('A'));
                    break;
                case PlaylistState::VideoOnly:
                    dragId.prepend(QLatin1Char('V'));
                    break;
                default:
                    break;
                }
            }
            list << dragId;
            duration += (std::static_pointer_cast<ProjectClip>(item))->frameDuration();
        } else if (type == AbstractProjectItem::SubClipItem) {
            QPoint p = item->zone();
            list << std::static_pointer_cast<ProjectSubClip>(item)->getMasterClip()->clipId() + QLatin1Char('/') + QString::number(p.x()) + QLatin1Char('/') +
                        QString::number(p.y());
        } else if (type == AbstractProjectItem::FolderItem) {
            list << "#" + item->clipId();
        }
    }
    if (!list.isEmpty()) {
        QByteArray data;
        data.append(list.join(QLatin1Char(';')).toUtf8());
        mimeData->setData(QStringLiteral("kdenlive/producerslist"), data);
        mimeData->setText(QString::number(duration));
    }
    return mimeData;
}

void ProjectItemModel::onItemUpdated(const std::shared_ptr<AbstractProjectItem> &item, int role)
{
    QWriteLocker locker(&m_lock);
    auto tItem = std::static_pointer_cast<TreeItem>(item);
    auto ptr = tItem->parentItem().lock();
    if (ptr) {
        auto index = getIndexFromItem(tItem);
        emit dataChanged(index, index, {role});
    }
}

void ProjectItemModel::onItemUpdated(const QString &binId, int role)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<AbstractProjectItem> item = getItemByBinId(binId);
    if (item) {
        onItemUpdated(item, role);
    }
}

std::shared_ptr<ProjectClip> ProjectItemModel::getClipByBinID(const QString &binId)
{
    READ_LOCK();
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

const QVector<uint8_t> ProjectItemModel::getAudioLevelsByBinID(const QString &binId, int stream)
{
    READ_LOCK();
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::ClipItem && c->clipId() == binId) {
            return std::static_pointer_cast<ProjectClip>(c)->audioFrameCache(stream);
        }
    }
    return QVector<uint8_t>();
}

bool ProjectItemModel::hasClip(const QString &binId)
{
    READ_LOCK();
    return getClipByBinID(binId) != nullptr;
}

std::shared_ptr<ProjectFolder> ProjectItemModel::getFolderByBinId(const QString &binId)
{
    READ_LOCK();
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::FolderItem && c->clipId() == binId) {
            return std::static_pointer_cast<ProjectFolder>(c);
        }
    }
    return nullptr;
}

QList <std::shared_ptr<ProjectFolder> > ProjectItemModel::getFolders()
{
    READ_LOCK();
    QList <std::shared_ptr<ProjectFolder> > folders;
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::FolderItem) {
            folders << std::static_pointer_cast<ProjectFolder>(c);
        }
    }
    return folders;
}

const QString ProjectItemModel::getFolderIdByName(const QString &folderName)
{
    READ_LOCK();
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::FolderItem && c->name() == folderName) {
            return c->clipId();
        }
    }
    return QString();
}

std::shared_ptr<AbstractProjectItem> ProjectItemModel::getItemByBinId(const QString &binId)
{
    READ_LOCK();
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
    QWriteLocker locker(&m_lock);
    return std::static_pointer_cast<AbstractProjectItem>(rootItem)->setBinEffectsEnabled(enabled);
}

QStringList ProjectItemModel::getEnclosingFolderInfo(const QModelIndex &index) const
{
    READ_LOCK();
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
    QWriteLocker locker(&m_lock);
    std::vector<std::shared_ptr<AbstractProjectItem>> toDelete;
    toDelete.reserve((size_t)rootItem->childCount());
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
    m_fileWatcher->clear();
}

std::shared_ptr<ProjectFolder> ProjectItemModel::getRootFolder() const
{
    READ_LOCK();
    return std::static_pointer_cast<ProjectFolder>(rootItem);
}

void ProjectItemModel::loadSubClips(const QString &id, const QString &clipData)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    loadSubClips(id, clipData, undo, redo);
}

void ProjectItemModel::loadSubClips(const QString &id, const QString &dataMap, Fun &undo, Fun &redo)
{
    if (dataMap.isEmpty()) {
        return;
    }
    QWriteLocker locker(&m_lock);
    std::shared_ptr<ProjectClip> clip = getClipByBinID(id);
    if (!clip) {
        qDebug()<<" = = = = = CLIP NOT LOADED";
        return;
    }
    auto json = QJsonDocument::fromJson(dataMap.toUtf8());
    if (!json.isArray()) {
        qDebug() << "Error loading zones : Json file should be an array";
        return;
    }
    int maxFrame = clip->duration().frames(pCore->getCurrentFps()) - 1;
    auto list = json.array();
    for (const auto &entry : qAsConst(list)) {
        if (!entry.isObject()) {
            qDebug() << "Warning : Skipping invalid marker data";
            continue;
        }
        auto entryObj = entry.toObject();
        if (!entryObj.contains(QLatin1String("name"))) {
            qDebug() << "Warning : Skipping invalid zone(does not contain name)";
            continue;
        }
        int in = entryObj[QLatin1String("in")].toInt();
        int out = entryObj[QLatin1String("out")].toInt();
        QMap <QString, QString> zoneProperties;
        zoneProperties.insert(QStringLiteral("name"), entryObj[QLatin1String("name")].toString(i18n("Zone")));
        zoneProperties.insert(QStringLiteral("rating"), QString::number(entryObj[QLatin1String("rating")].toInt()));
        zoneProperties.insert(QStringLiteral("tags"), entryObj[QLatin1String("tags")].toString(QString()));
        if (in >= out) {
            qDebug() << "Warning : Invalid zone: "<<zoneProperties.value("name")<<", "<<in<<"-"<<out;
            continue;
        }
        if (maxFrame > 0) {
            out = qMin(out, maxFrame);
        }
        QString subId;
        requestAddBinSubClip(subId, in, out, zoneProperties, id, undo, redo);
    }
}

std::shared_ptr<AbstractProjectItem> ProjectItemModel::getBinItemByIndex(const QModelIndex &index) const
{
    READ_LOCK();
    return std::static_pointer_cast<AbstractProjectItem>(getItemById((int)index.internalId()));
}

bool ProjectItemModel::requestBinClipDeletion(const std::shared_ptr<AbstractProjectItem> &clip, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(clip);
    if (!clip) return false;
    int parentId = -1;
    QString binId;
    if (auto ptr = clip->parent()) {
        parentId = ptr->getId();
        binId = ptr->clipId();
    }
    bool isSubClip = clip->itemType() == AbstractProjectItem::SubClipItem;
    clip->selfSoftDelete(undo, redo);
    int id = clip->getId();
    Fun operation = removeItem_lambda(id);
    Fun reverse = addItem_lambda(clip, parentId);
    bool res = operation();
    if (res) {
        if (isSubClip) {
            Fun update_doc = [this, binId]() {
                std::shared_ptr<AbstractProjectItem> parentItem = getItemByBinId(binId);
                if (parentItem && parentItem->itemType() == AbstractProjectItem::ClipItem) {
                    auto clipItem = std::static_pointer_cast<ProjectClip>(parentItem);
                    clipItem->updateZones();
                }
                return true;
            };
            update_doc();
            PUSH_LAMBDA(update_doc, operation);
            PUSH_LAMBDA(update_doc, reverse);
        }
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    }
    return res;
}

void ProjectItemModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    QWriteLocker locker(&m_lock);
    auto clip = std::static_pointer_cast<AbstractProjectItem>(item);
    m_binPlaylist->manageBinItemInsertion(clip);
    AbstractTreeModel::registerItem(item);
    if (clip->itemType() == AbstractProjectItem::ClipItem) {
        auto clipItem = std::static_pointer_cast<ProjectClip>(clip);
        updateWatcher(clipItem);
    }
}
void ProjectItemModel::deregisterItem(int id, TreeItem *item)
{
    QWriteLocker locker(&m_lock);
    auto clip = static_cast<AbstractProjectItem *>(item);
    m_binPlaylist->manageBinItemDeletion(clip);
    // TODO : here, we should suspend jobs belonging to the item we delete. They can be restarted if the item is reinserted by undo
    AbstractTreeModel::deregisterItem(id, item);
    if (clip->itemType() == AbstractProjectItem::ClipItem) {
        auto clipItem = static_cast<ProjectClip *>(clip);
        m_fileWatcher->removeFile(clipItem->clipId());
    }
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

bool ProjectItemModel::addItem(const std::shared_ptr<AbstractProjectItem> &item, const QString &parentId, Fun &undo, Fun &redo)
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
        id.clear();
    }
    if (id.isEmpty()) {
        id = QString::number(getFreeFolderId());
    }
    std::shared_ptr<ProjectFolder> new_folder = ProjectFolder::construct(id, name, std::static_pointer_cast<ProjectItemModel>(shared_from_this()));
    return addItem(new_folder, parentId, undo, redo);
}

bool ProjectItemModel::requestAddBinClip(QString &id, const QDomElement &description, const QString &parentId, Fun &undo, Fun &redo,
                                         const std::function<void(const QString &)> &readyCallBack)
{
    qDebug() << "/////////// requestAddBinClip" << parentId;
    QWriteLocker locker(&m_lock);
    if (id.isEmpty()) {
        id =
            Xml::getXmlProperty(description, QStringLiteral("kdenlive:id"), QStringLiteral("-1"));
        if (id == QStringLiteral("-1") || !isIdFree(id)) {
            id = QString::number(getFreeClipId());
        }
    }
    Q_ASSERT(isIdFree(id));
    qDebug() << "/////////// found id" << id;
    std::shared_ptr<ProjectClip> new_clip =
        ProjectClip::construct(id, description, m_blankThumb, std::static_pointer_cast<ProjectItemModel>(shared_from_this()));
    qDebug() << "/////////// constructed ";
    bool res = addItem(new_clip, parentId, undo, redo);
    qDebug() << "/////////// added " << res;
    if (res) {
        int loadJob = emit pCore->jobManager()->startJob<LoadJob>({id}, -1, QString(), description, std::bind(readyCallBack, id));
        emit pCore->jobManager()->startJob<ThumbJob>({id}, loadJob, QString(), 0, true);
        ClipType::ProducerType type = new_clip->clipType();
        if (KdenliveSettings::audiothumbnails()) {
            if (type == ClipType::AV || type == ClipType::Audio || type == ClipType::Playlist || type == ClipType::Unknown) {
                emit pCore->jobManager()->startJob<AudioThumbJob>({id}, loadJob, QString());
            }
        }
    }
    return res;
}

bool ProjectItemModel::requestAddBinClip(QString &id, const QDomElement &description, const QString &parentId, const QString &undoText)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestAddBinClip(id, description, parentId, undo, redo);
    if (res) {
        pCore->pushUndo(undo, redo, undoText.isEmpty() ? i18n("Add bin clip") : undoText);
    }
    return res;
}

bool ProjectItemModel::requestAddBinClip(QString &id, const std::shared_ptr<Mlt::Producer> &producer, const QString &parentId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (id.isEmpty()) {
        id = QString::number(producer->get_int("kdenlive:id"));
        if (!isIdFree(id)) {
            id = QString::number(getFreeClipId());
        }
    }
    Q_ASSERT(isIdFree(id));
    std::shared_ptr<ProjectClip> new_clip = ProjectClip::construct(id, m_blankThumb, std::static_pointer_cast<ProjectItemModel>(shared_from_this()), producer);
    bool res = addItem(new_clip, parentId, undo, redo);
    if (res) {
        new_clip->importEffects(producer);
        if (new_clip->isReady() || new_clip->sourceExists()) {
            int blocking = pCore->jobManager()->getBlockingJobId(id, AbstractClipJob::LOADJOB);
            emit pCore->jobManager()->startJob<ThumbJob>({id}, blocking, QString(), -1, true);
            if (KdenliveSettings::audiothumbnails()) {
                emit pCore->jobManager()->startJob<AudioThumbJob>({id}, blocking, QString());
            }
        }
    }
    return res;
}

bool ProjectItemModel::requestAddBinSubClip(QString &id, int in, int out, const QMap<QString, QString> zoneProperties, const QString &parentId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (id.isEmpty()) {
        id = QString::number(getFreeClipId());
    }
    Q_ASSERT(isIdFree(id));
    QString subId = parentId;
    if (subId.startsWith(QLatin1Char('A')) || subId.startsWith(QLatin1Char('V'))) {
        subId.remove(0, 1);
    }
    auto clip = getClipByBinID(subId);
    Q_ASSERT(clip->itemType() == AbstractProjectItem::ClipItem);
    auto tc = pCore->currentDoc()->timecode().getDisplayTimecodeFromFrames(in, KdenliveSettings::frametimecode());
    std::shared_ptr<ProjectSubClip> new_clip =
        ProjectSubClip::construct(id, clip, std::static_pointer_cast<ProjectItemModel>(shared_from_this()), in, out, tc, zoneProperties);
    bool res = addItem(new_clip, subId, undo, redo);
    if (res) {
        int parentJob = pCore->jobManager()->getBlockingJobId(subId, AbstractClipJob::LOADJOB);
        emit pCore->jobManager()->startJob<ThumbJob>({id}, parentJob, QString(), -1, true);
    }
    return res;
}
bool ProjectItemModel::requestAddBinSubClip(QString &id, int in, int out, const QMap<QString, QString> zoneProperties, const QString &parentId)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestAddBinSubClip(id, in, out, zoneProperties, parentId, undo, redo);
    if (res) {
        Fun update_doc = [this, parentId]() {
            std::shared_ptr<AbstractProjectItem> parentItem = getItemByBinId(parentId);
            if (parentItem && parentItem->itemType() == AbstractProjectItem::ClipItem) {
                auto clipItem = std::static_pointer_cast<ProjectClip>(parentItem);
                clipItem->updateZones();
            }
            return true;
        };
        update_doc();
        PUSH_LAMBDA(update_doc, undo);
        PUSH_LAMBDA(update_doc, redo);
        pCore->pushUndo(undo, redo, i18n("Add a sub clip"));
    }
    return res;
}

Fun ProjectItemModel::requestRenameFolder_lambda(const std::shared_ptr<AbstractProjectItem> &folder, const QString &newName)
{
    int id = folder->getId();
    return [this, id, newName]() {
        auto currentFolder = std::static_pointer_cast<AbstractProjectItem>(m_allItems[id].lock());
        if (!currentFolder) {
            return false;
        }
        currentFolder->setName(newName);
        m_binPlaylist->manageBinFolderRename(currentFolder);
        auto index = getIndexFromItem(currentFolder);
        emit dataChanged(index, index, {AbstractProjectItem::DataName});
        return true;
    };
}

bool ProjectItemModel::requestRenameFolder(const std::shared_ptr<AbstractProjectItem> &folder, const QString &name, Fun &undo, Fun &redo)
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
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestRenameFolder(std::move(folder), name, undo, redo);
    if (res) {
        pCore->pushUndo(undo, redo, i18n("Rename Folder"));
    }
    return res;
}

bool ProjectItemModel::requestCleanup()
{
    QWriteLocker locker(&m_lock);
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
    READ_LOCK();
    std::vector<QString> result;
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::ClipItem) {
            result.push_back(c->clipId());
        }
    }
    return result;
}

QStringList ProjectItemModel::getClipByUrl(const QFileInfo &url) const
{
    READ_LOCK();
    QStringList result;
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::ClipItem) {
            if (QFileInfo(std::static_pointer_cast<ProjectClip>(c)->clipUrl()) == url) {
                result << c->clipId();
            }
        }
    }
    return result;
}

bool ProjectItemModel::loadFolders(Mlt::Properties &folders, std::unordered_map<QString, QString> &binIdCorresp)
{
    QWriteLocker locker(&m_lock);
    // At this point, we expect the folders properties to have a name of the form "x.y" where x is the id of the parent folder and y the id of the child.
    // Note that for root folder, x = -1
    // The value of the property is the name of the child folder

    std::unordered_map<int, std::vector<int>> downLinks; // key are parents, value are children
    std::unordered_map<int, int> upLinks;                // key are children, value are parent
    std::unordered_map<int, QString> newIds;             // we store the correspondence to the new ids
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

    // In case there are some non-existent parent, we fall back to root
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
            binIdCorresp[QString::number(current)] = id;
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
    READ_LOCK();
    if (id.isEmpty()) {
        return false;
    }
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->clipId() == id) {
            return false;
        }
    }
    return true;
}

void ProjectItemModel::loadBinPlaylist(Mlt::Tractor *documentTractor, Mlt::Tractor *modelTractor, std::unordered_map<QString, QString> &binIdCorresp, QStringList &expandedFolders, QProgressDialog *progressDialog)
{
    QWriteLocker locker(&m_lock);
    clean();
    Mlt::Properties retainList((mlt_properties)documentTractor->get_data("xml_retain"));
    qDebug() << "Loading bin playlist...";
    if (retainList.is_valid()) {
        qDebug() << "retain is valid";
        Mlt::Playlist playlist((mlt_playlist)retainList.get_data(BinPlaylist::binPlaylistId.toUtf8().constData()));
        if (playlist.is_valid() && playlist.type() == playlist_type) {
            qDebug() << "playlist is valid";
            if (progressDialog == nullptr && playlist.count() > 0) {
                // Display message on splash screen
                emit pCore->loadingMessageUpdated(i18n("Loading project clips..."));
            }
            // Load bin clips
            auto currentLocale = strdup(setlocale(LC_ALL, nullptr));
            qDebug() << "Init bin; Current LC_ALL" << currentLocale;
            // Load folders
            Mlt::Properties folderProperties;
            Mlt::Properties playlistProps(playlist.get_properties());
            expandedFolders = QString(playlistProps.get("kdenlive:expandedFolders")).split(QLatin1Char(';'));
            folderProperties.pass_values(playlistProps, "kdenlive:folder.");
            loadFolders(folderProperties, binIdCorresp);

            // Read notes
            QString notes = playlistProps.get("kdenlive:documentnotes");
            pCore->projectManager()->setDocumentNotes(notes);

            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            qDebug() << "Found " << playlist.count() << "clips";
            int max = playlist.count();
            if (progressDialog) {
                progressDialog->setMaximum(progressDialog->maximum() + max);
            }
            QMap <int, std::shared_ptr<Mlt::Producer> > binProducers;
            for (int i = 0; i < max; i++) {
                if (progressDialog) {
                    progressDialog->setValue(i);
                } else {
                    emit pCore->loadingMessageUpdated(QString(), 1);
                }
                QScopedPointer<Mlt::Producer> prod(playlist.get_clip(i));
                if (prod->is_blank() || !prod->is_valid()) {
                    qDebug() << "producer is not valid or blank";
                    continue;
                }
                std::shared_ptr<Mlt::Producer> producer(new Mlt::Producer(prod->parent()));
                int id = producer->get_int("kdenlive:id");
                if (!id) id = getFreeClipId();
                binProducers.insert(id, producer);
            }
            // Do the real insertion
            QMapIterator<int, std::shared_ptr<Mlt::Producer> > i(binProducers);
            while (i.hasNext()) {
                i.next();
                QString newId = QString::number(getFreeClipId());
                QString parentId = qstrdup(i.value()->get("kdenlive:folderid"));
                if (parentId.isEmpty()) {
                    parentId = QStringLiteral("-1");
                }
                i.value()->set("_kdenlive_processed", 1);
                requestAddBinClip(newId, std::move(i.value()), parentId, undo, redo);
                binIdCorresp[QString::number(i.key())] = newId;
                qDebug() << "Loaded clip " << i.key() << "under id" << newId;
            }
        }
    }
    m_binPlaylist->setRetainIn(modelTractor);
}

/** @brief Save document properties in MLT's bin playlist */
void ProjectItemModel::saveDocumentProperties(const QMap<QString, QString> &props, const QMap<QString, QString> &metadata,
                                              std::shared_ptr<MarkerListModel> guideModel)
{
    m_binPlaylist->saveDocumentProperties(props, metadata, std::move(guideModel));
}

void ProjectItemModel::saveProperty(const QString &name, const QString &value)
{
    m_binPlaylist->saveProperty(name, value);
}

QMap<QString, QString> ProjectItemModel::getProxies(const QString &root)
{
    READ_LOCK();
    return m_binPlaylist->getProxies(root);
}

void ProjectItemModel::reloadClip(const QString &binId)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<ProjectClip> clip = getClipByBinID(binId);
    if (clip) {
        clip->reloadProducer();
    }
}

void ProjectItemModel::setClipWaiting(const QString &binId)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<ProjectClip> clip = getClipByBinID(binId);
    if (clip) {
        clip->setClipStatus(AbstractProjectItem::StatusWaiting);
    }
}

void ProjectItemModel::setClipInvalid(const QString &binId)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<ProjectClip> clip = getClipByBinID(binId);
    if (clip) {
        clip->setClipStatus(AbstractProjectItem::StatusMissing);
        // TODO: set producer as blank invalid
    }
}

void ProjectItemModel::updateWatcher(const std::shared_ptr<ProjectClip> &clipItem)
{
    QWriteLocker locker(&m_lock);
    if (clipItem->clipType() == ClipType::AV || clipItem->clipType() == ClipType::Audio || clipItem->clipType() == ClipType::Image ||
        clipItem->clipType() == ClipType::Video || clipItem->clipType() == ClipType::Playlist || clipItem->clipType() == ClipType::TextTemplate) {
        m_fileWatcher->removeFile(clipItem->clipId());
        QFileInfo check_file(clipItem->clipUrl());
        // check if file exists and if yes: Is it really a file and no directory?
        if ((check_file.exists() && check_file.isFile()) || clipItem->clipStatus() == AbstractProjectItem::StatusMissing) {
            m_fileWatcher->addFile(clipItem->clipId(), clipItem->clipUrl());
        }
    }
}

void ProjectItemModel::setDragType(PlaylistState::ClipState type)
{
    QWriteLocker locker(&m_lock);
    m_dragType = type;
}

int ProjectItemModel::clipsCount() const
{
    READ_LOCK();
    return m_binPlaylist->count();
}

bool ProjectItemModel::validateClip(const QString &binId, const QString &clipHash)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<ProjectClip> clip = getClipByBinID(binId);
    if (clip) {
        return clip->hash() == clipHash;
    }
    return false;
}

QString ProjectItemModel::validateClipInFolder(const QString &folderId, const QString &clipHash)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<ProjectFolder> folder = getFolderByBinId(folderId);
    if (folder) {
        return folder->childByHash(clipHash);
    }
    return QString();
}
