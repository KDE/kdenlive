/*
SPDX-FileCopyrightText: 2012 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
SPDX-FileCopyrightText: 2017 Nicolas Carion
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "projectitemmodel.h"
#include "abstractprojectitem.h"
#include "binplaylist.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "filewatcher.hpp"
#include "jobs/audiolevelstask.h"
#include "jobs/cliploadtask.h"
#include "kdenlivesettings.h"
#include "lib/localeHandling.h"
#include "macros.hpp"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "projectclip.h"
#include "projectfolder.h"
#include "projectsubclip.h"
#include "utils/thumbnailcache.hpp"
#include "xml/xml.hpp"

#include <KLocalizedString>

#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QProgressDialog>

#include <mlt++/Mlt.h>
#include <queue>
#include <qvarlengtharray.h>
#include <utility>

ProjectItemModel::ProjectItemModel(QObject *parent)
    : AbstractTreeModel(parent)
    , closing(false)
    , m_lock(QReadWriteLock::Recursive)
    , m_binPlaylist(nullptr)
    , m_fileWatcher(new FileWatcher())
    , m_nextId(1)
    , m_blankThumb()
    , m_dragType(PlaylistState::Disabled)
    , m_uuid(QUuid::createUuid())
    , m_sequenceFolderId(-1)
{
    QPixmap pix(QSize(160, 90));
    pix.fill(Qt::lightGray);
    m_blankThumb.addPixmap(pix);
    connect(m_fileWatcher.get(), &FileWatcher::binClipModified, this, &ProjectItemModel::reloadClip);
    connect(m_fileWatcher.get(), &FileWatcher::binClipWaiting, this, &ProjectItemModel::setClipWaiting);
    connect(m_fileWatcher.get(), &FileWatcher::binClipMissing, this, &ProjectItemModel::setClipInvalid);
    missingClipTimer.setInterval(500);
    missingClipTimer.setSingleShot(true);
    connect(&missingClipTimer, &QTimer::timeout, this, &ProjectItemModel::slotUpdateInvalidCount);
}

std::shared_ptr<ProjectItemModel> ProjectItemModel::construct(QObject *parent)
{
    std::shared_ptr<ProjectItemModel> self(new ProjectItemModel(parent));
    self->rootItem = ProjectFolder::construct(self);
    return self;
}

ProjectItemModel::~ProjectItemModel() = default;

void ProjectItemModel::buildPlaylist(const QUuid uuid)
{
    m_uuid = uuid;
    QPixmap pix;
    if (pCore->getCurrentDar() > 1) {
        pix = QPixmap(QSize(160, 160 / pCore->getCurrentDar()));
    } else {
        pix = QPixmap(QSize(90 * pCore->getCurrentDar(), 90));
    }
    pix.fill(Qt::lightGray);
    m_blankThumb = QIcon();
    m_blankThumb.addPixmap(pix);
    m_fileWatcher->clear();
    m_extraPlaylists.clear();
    Q_ASSERT(m_projectTractor.use_count() <= 1);
    m_projectTractor.reset();
    m_binPlaylist.reset(new BinPlaylist(uuid));
    m_projectTractor.reset(new Mlt::Tractor(pCore->getProjectProfile()));
    m_projectTractor->set("kdenlive:projectTractor", 1);
    m_binPlaylist->setRetainIn(m_projectTractor.get());
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
    case 8:
        return AbstractProjectItem::UsageCount;
        break;
    default:
        return AbstractProjectItem::DataName;
    }
}

QList<int> ProjectItemModel::mapDataToColumn(AbstractProjectItem::DataType type) const
{
    // Some data types are used in several columns, for example usage count has its
    // own column for sorting but is also displayed in column 0
    switch (type) {
    case AbstractProjectItem::DataName:
    case AbstractProjectItem::DataThumbnail:
    case AbstractProjectItem::IconOverlay:
    case AbstractProjectItem::JobProgress:
        return {0};
        break;
    case AbstractProjectItem::DataDate:
        return {1};
        break;
    case AbstractProjectItem::DataDescription:
        return {2};
        break;
    case AbstractProjectItem::ClipType:
        return {3};
        break;
    case AbstractProjectItem::DataTag:
        return {0, 4};
        break;
    case AbstractProjectItem::DataDuration:
        return {0, 5};
        break;
    case AbstractProjectItem::DataId:
        return {6};
        break;
    case AbstractProjectItem::DataRating:
        return {7};
        break;
    case AbstractProjectItem::UsageCount:
        return {0, 8};
        break;
    default:
        return {};
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
        return item->icon();
    }
    std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(index);
    return item->getData(static_cast<AbstractProjectItem::DataType>(role));
}

bool ProjectItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(index);
    if (item->rename(value.toString(), index.column())) {
        Q_EMIT dataChanged(index, index, {role});
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
        Q_EMIT urlsDropped(data->urls(), parent);
        return true;
    }

    if (data->hasFormat(QStringLiteral("text/producerslist"))) {
        // Dropping an Bin item
        const QStringList ids = QString(data->data(QStringLiteral("text/producerslist"))).split(QLatin1Char(';'));
        if (ids.constFirst().contains(QLatin1Char('/'))) {
            // subclip zone
            QStringList clipData = ids.constFirst().split(QLatin1Char('/'));
            if (clipData.length() >= 3) {
                QString bid = clipData.at(0);
                if (bid.startsWith(QLatin1Char('A')) || bid.startsWith(QLatin1Char('V'))) {
                    bid.remove(0, 1);
                }
                std::shared_ptr<ProjectClip> masterClip = getClipByBinID(bid);
                std::shared_ptr<ProjectSubClip> sub = masterClip->getSubClip(clipData.at(1).toInt(), clipData.at(2).toInt());
                if (sub != nullptr) {
                    // This zone already exists
                    return false;
                }
                QString id;
                return requestAddBinSubClip(id, clipData.at(1).toInt(), clipData.at(2).toInt(), {}, bid);
            } else {
                // error, malformed clip zone, abort
                return false;
            }
        } else {
            Q_EMIT itemDropped(ids, parent);
        }
        return true;
    }

    if (data->hasFormat(QStringLiteral("kdenlive/effect"))) {
        // Dropping effect on a Bin item
        QStringList effectData;
        effectData << QString::fromUtf8(data->data(QStringLiteral("kdenlive/effect")));
        QStringList source = QString::fromUtf8(data->data(QStringLiteral("kdenlive/effectsource"))).split(QLatin1Char(','));
        effectData << source;
        Q_EMIT effectDropped(effectData, parent);
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
        Q_EMIT addTag(tag, parent);
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
        case 8:
            columnName = i18n("Usage");
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
    QStringList types{QStringLiteral("text/producerslist"), QStringLiteral("text/uri-list"), QStringLiteral("kdenlive/clip"), QStringLiteral("kdenlive/effect"),
                      QStringLiteral("kdenlive/tag")};
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
    QString parentId;
    size_t duration = 0;
    for (int i = 0; i < indices.count(); i++) {
        QModelIndex ix = indices.at(i);
        if (!ix.isValid() || ix.column() != 0) {
            continue;
        }
        std::shared_ptr<AbstractProjectItem> item = getBinItemByIndex(ix);
        if (!item->statusReady()) {
            continue;
        }
        if (parentId.isEmpty()) {
            parentId = ix.parent().data(AbstractProjectItem::DataId).toString();
        }
        AbstractProjectItem::PROJECTITEMTYPE type = item->itemType();
        if (type == AbstractProjectItem::ClipItem) {
            ClipType::ProducerType cType = item->clipType();
            QString dragId = item->clipId();
            if ((cType == ClipType::AV || cType == ClipType::Playlist || cType == ClipType::Timeline)) {
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
            std::shared_ptr<ProjectClip> master = std::static_pointer_cast<ProjectSubClip>(item)->getMasterClip();
            QString dragId = master->clipId();
            ClipType::ProducerType cType = master->clipType();
            if ((cType == ClipType::AV || cType == ClipType::Playlist || cType == ClipType::Timeline)) {
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
            list << dragId + QLatin1Char('/') + QString::number(p.x()) + QLatin1Char('/') + QString::number(p.y());
        } else if (type == AbstractProjectItem::FolderItem) {
            list << "#" + item->clipId();
        }
    }
    if (!list.isEmpty()) {
        QByteArray data;
        data.append(list.join(QLatin1Char(';')).toUtf8());
        mimeData->setData(QStringLiteral("text/producerslist"), data);
        mimeData->setData(QStringLiteral("text/rootId"), parentId.toLatin1());
        mimeData->setData(QStringLiteral("text/dragid"), QUuid::createUuid().toByteArray());
        mimeData->setText(QString::number(duration));
    }
    return mimeData;
}

void ProjectItemModel::onItemUpdated(const std::shared_ptr<AbstractProjectItem> &item, const QVector<int> &roles)
{
    int minColumn = -1;
    int maxColumn = -1;
    for (auto &r : roles) {
        const QList<int> indexes = mapDataToColumn((AbstractProjectItem::DataType)r);
        for (auto &ix : indexes) {
            if (minColumn == -1 || ix < minColumn) {
                minColumn = ix;
            }
            if (maxColumn == -1 || ix > maxColumn) {
                maxColumn = ix;
            }
        }
    }
    if (minColumn == -1) {
        return;
    }
    QWriteLocker locker(&m_lock);
    auto tItem = std::static_pointer_cast<TreeItem>(item);
    auto ptr = tItem->parentItem().lock();
    if (ptr) {
        auto index = getIndexFromItem(tItem, minColumn);
        if (minColumn == maxColumn) {
            Q_EMIT dataChanged(index, index, roles);
        } else {
            auto index2 = getIndexFromItem(tItem, maxColumn);
            Q_EMIT dataChanged(index, index2, roles);
        }
    }
}

void ProjectItemModel::onItemUpdated(const QString &binId, int role)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<AbstractProjectItem> item = getItemByBinId(binId);
    if (item) {
        onItemUpdated(item, {role});
    }
}

std::shared_ptr<ProjectClip> ProjectItemModel::getClipByBinID(const QString &binId)
{
    READ_LOCK();
    auto search = m_allClipItems.find(binId.toInt());
    if (search != m_allClipItems.end()) {
        return search->second;
    }
    return nullptr;
}

const QVector<uint8_t> ProjectItemModel::getAudioLevelsByBinID(const QString &binId, int stream)
{
    READ_LOCK();
    auto search = m_allClipItems.find(binId.toInt());
    if (search != m_allClipItems.end()) {
        return search->second->audioFrameCache(stream);
    }
    return QVector<uint8_t>();
}

double ProjectItemModel::getAudioMaxLevel(const QString &binId, int stream)
{
    READ_LOCK();
    auto search = m_allClipItems.find(binId.toInt());
    if (search != m_allClipItems.end()) {
        return search->second->getAudioMax(stream);
    }
    return 0;
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

QList<std::shared_ptr<ProjectFolder>> ProjectItemModel::getFolders()
{
    READ_LOCK();
    QList<std::shared_ptr<ProjectFolder>> folders;
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
    closing = true;
    m_extraPlaylists.clear();
    std::vector<std::shared_ptr<AbstractProjectItem>> toDelete;
    toDelete.reserve(size_t(rootItem->childCount()));
    for (int i = 0; i < rootItem->childCount(); ++i) {
        qDebug() << "... FOUND CLIP: " << std::static_pointer_cast<AbstractProjectItem>(rootItem->child(i))->clipId() << " = "
                 << std::static_pointer_cast<AbstractProjectItem>(rootItem->child(i))->name();
        toDelete.push_back(std::static_pointer_cast<AbstractProjectItem>(rootItem->child(i)));
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    for (const auto &child : toDelete) {
        requestBinClipDeletion(child, undo, redo);
    }
    Q_ASSERT(rootItem->childCount() == 0);
    closing = false;
    m_nextId = 1;
    m_uuid = QUuid::createUuid();
    m_sequenceFolderId = -1;
    buildPlaylist(m_uuid);
    ThumbnailCache::get()->clearCache();
}

std::shared_ptr<ProjectFolder> ProjectItemModel::getRootFolder() const
{
    READ_LOCK();
    return std::static_pointer_cast<ProjectFolder>(rootItem);
}

void ProjectItemModel::loadSubClips(const QString &id, const QString &clipData, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    loadSubClips(id, clipData, undo, redo);
    if (logUndo) {
        Fun update_subs = [this, binId = id]() {
            std::shared_ptr<AbstractProjectItem> parentItem = getItemByBinId(binId);
            if (parentItem && parentItem->itemType() == AbstractProjectItem::ClipItem) {
                auto clipItem = std::static_pointer_cast<ProjectClip>(parentItem);
                clipItem->updateZones();
            }
            return true;
        };
        PUSH_LAMBDA(update_subs, undo);
        PUSH_LAMBDA(update_subs, redo);
        update_subs();
        pCore->pushUndo(undo, redo, i18n("Add sub clips"));
    }
}

void ProjectItemModel::loadSubClips(const QString &id, const QString &dataMap, Fun &undo, Fun &redo)
{
    if (dataMap.isEmpty()) {
        return;
    }
    QWriteLocker locker(&m_lock);
    std::shared_ptr<ProjectClip> clip = getClipByBinID(id);
    if (!clip) {
        qWarning() << "Clip not loaded";
        return;
    }
    auto json = QJsonDocument::fromJson(dataMap.toUtf8());
    if (!json.isArray()) {
        qWarning() << "Error loading zones: no json array";
        return;
    }
    int maxFrame = clip->duration().frames(pCore->getCurrentFps()) - 1;
    auto list = json.array();
    for (const auto &entry : qAsConst(list)) {
        if (!entry.isObject()) {
            qWarning() << "Skipping invalid marker data";
            continue;
        }
        auto entryObj = entry.toObject();
        if (!entryObj.contains(QLatin1String("name"))) {
            qWarning() << "Skipping invalid zone (does not contain name)";
            continue;
        }
        int in = entryObj[QLatin1String("in")].toInt();
        int out = entryObj[QLatin1String("out")].toInt();
        QMap<QString, QString> zoneProperties;
        zoneProperties.insert(QStringLiteral("name"), entryObj[QLatin1String("name")].toString(i18n("Zone")));
        zoneProperties.insert(QStringLiteral("rating"), QString::number(entryObj[QLatin1String("rating")].toInt()));
        zoneProperties.insert(QStringLiteral("tags"), entryObj[QLatin1String("tags")].toString(QString()));
        if (in >= out) {
            qWarning() << "Invalid zone" << zoneProperties.value("name") << in << out;
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
    return std::static_pointer_cast<AbstractProjectItem>(getItemById(int(index.internalId())));
}

bool ProjectItemModel::requestBinClipDeletion(const std::shared_ptr<AbstractProjectItem> &clip, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(clip);
    if (!clip) return false;
    int parentId = -1;
    QString parentBinId;
    int binId = clip->clipId().toInt();
    if (binId == m_sequenceFolderId) {
        setSequencesFolder(-1);
    }
    if (auto ptr = clip->parent()) {
        parentId = ptr->getId();
        parentBinId = ptr->clipId();
    }
    bool isSubClip = clip->itemType() == AbstractProjectItem::SubClipItem;
    if (!clip->selfSoftDelete(undo, redo)) {
        return false;
    }
    int id = clip->getId();
    Fun operation = removeProjectItem_lambda(binId, id);
    Fun reverse = addItem_lambda(clip, parentId);
    bool res = operation();
    if (res) {
        if (isSubClip) {
            Fun update_doc = [this, parentBinId]() {
                std::shared_ptr<AbstractProjectItem> parentItem = getItemByBinId(parentBinId);
                if (parentItem && parentItem->itemType() == AbstractProjectItem::ClipItem) {
                    auto clipItem = std::static_pointer_cast<ProjectClip>(parentItem);
                    clipItem->updateZones();
                }
                return true;
            };
            update_doc();
            PUSH_LAMBDA(update_doc, operation);
            PUSH_LAMBDA(update_doc, reverse);
        } else {
            Fun checkAudio = clip->getAudio_lambda();
            PUSH_LAMBDA(checkAudio, reverse);
        }
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    }
    return res;
}

void ProjectItemModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    QWriteLocker locker(&m_lock);
    AbstractTreeModel::registerItem(item);
    auto clip = std::static_pointer_cast<AbstractProjectItem>(item);
    if (clip == nullptr || clip->clipId().toInt() == -1) {
        // Root item, no need to register it
        return;
    }
    Q_ASSERT(m_binPlaylist != nullptr);
    m_binPlaylist->manageBinItemInsertion(clip);
    m_allIds.append(clip->clipId().toInt());
    if (clip->itemType() == AbstractProjectItem::ClipItem) {
        auto clipItem = std::static_pointer_cast<ProjectClip>(clip);
        m_allClipItems[clip->clipId().toInt()] = clipItem;
        updateWatcher(clipItem);
        if (clipItem->clipType() == ClipType::Timeline && clipItem->statusReady()) {
            const QString uuid = clipItem->getSequenceUuid().toString();
            std::shared_ptr<Mlt::Tractor> trac(new Mlt::Tractor(clipItem->originalProducer()->parent()));
            storeSequence(uuid, trac, false);
        }
    }
}

void ProjectItemModel::deregisterItem(int id, TreeItem *item)
{
    QWriteLocker locker(&m_lock);
    auto clip = static_cast<AbstractProjectItem *>(item);
    m_allIds.removeAll(clip->clipId().toInt());
    m_allClipItems.erase(clip->clipId().toInt());
    m_binPlaylist->manageBinItemDeletion(clip);
    // TODO : here, we should suspend jobs belonging to the item we delete. They can be restarted if the item is reinserted by undo
    AbstractTreeModel::deregisterItem(id, item);
    if (clip->itemType() == AbstractProjectItem::ClipItem) {
        auto clipItem = static_cast<ProjectClip *>(clip);
        m_fileWatcher->removeFile(clipItem->clipId());
        if (clipItem->clipType() == ClipType::Timeline) {
            const QString uuid = clipItem->getSequenceUuid().toString();
            if (m_extraPlaylists.count(uuid) > 0) {
                m_extraPlaylists.erase(uuid);
            }
        }
    }
}

bool ProjectItemModel::hasSequenceId(const QUuid &uuid) const
{
    return m_binPlaylist->hasSequenceId(uuid);
}

std::shared_ptr<ProjectClip> ProjectItemModel::getSequenceClip(const QUuid &uuid)
{
    const QString binId = getSequenceId(uuid);
    return getClipByBinID(binId);
}

QMap<QUuid, QString> ProjectItemModel::getAllSequenceClips() const
{
    return m_binPlaylist->getAllSequenceClips();
}

const QString ProjectItemModel::getSequenceId(const QUuid &uuid)
{
    return m_binPlaylist->getSequenceId(uuid);
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
    int binId = item->clipId().toInt();
    Fun reverse = removeProjectItem_lambda(binId, itemId);
    bool res = operation();
    Q_ASSERT(item->isInModel());
    if (res) {
        Fun checkAudio = item->getAudio_lambda();
        PUSH_LAMBDA(checkAudio, operation);
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
    if (id.isEmpty()) {
        id = Xml::getXmlProperty(description, QStringLiteral("kdenlive:id"), QStringLiteral("-1"));
        if (id == QStringLiteral("-1") || !isIdFree(id)) {
            id = QString::number(getFreeClipId());
        }
    }
    Q_ASSERT(isIdFree(id));
    QWriteLocker locker(&m_lock);
    std::shared_ptr<ProjectClip> new_clip =
        ProjectClip::construct(id, description, m_blankThumb, std::static_pointer_cast<ProjectItemModel>(shared_from_this()));
    locker.unlock();
    bool res = addItem(new_clip, parentId, undo, redo);
    if (res) {
        ClipLoadTask::start(ObjectId(KdenliveObjectType::BinClip, id.toInt(), QUuid()), description, false, -1, -1, this, false, std::bind(readyCallBack, id));
    }
    return res;
}

bool ProjectItemModel::requestAddBinClip(QString &id, const QDomElement &description, const QString &parentId, const QString &undoText,
                                         const std::function<void(const QString &)> &readyCallBack)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestAddBinClip(id, description, parentId, undo, redo, readyCallBack);
    if (res) {
        pCore->pushUndo(undo, redo, undoText.isEmpty() ? i18n("Add bin clip") : undoText);
    }
    return res;
}

bool ProjectItemModel::requestAddBinClip(QString &id, std::shared_ptr<Mlt::Producer> &producer, const QString &parentId, Fun &undo, Fun &redo,
                                         const std::function<void(const QString &)> &readyCallBack)
{
    QWriteLocker locker(&m_lock);
    if (id.isEmpty()) {
        if (producer->property_exists("kdenlive:id")) {
            id = QString::number(producer->get_int("kdenlive:id"));
        }
        if (!isIdFree(id)) {
            id = QString::number(getFreeClipId());
        }
    }
    Q_ASSERT(isIdFree(id));
    std::shared_ptr<ProjectClip> new_clip = ProjectClip::construct(id, m_blankThumb, std::static_pointer_cast<ProjectItemModel>(shared_from_this()), producer);
    bool res = addItem(new_clip, parentId, undo, redo);
    if (res) {
        new_clip->importEffects(producer);
        const std::function<void()> readyCallBackExec = std::bind(readyCallBack, id);
        QMetaObject::invokeMethod(qApp, [readyCallBackExec] { readyCallBackExec(); });
    }
    return res;
}

bool ProjectItemModel::requestAddBinSubClip(QString &id, int in, int out, const QMap<QString, QString> &zoneProperties, const QString &parentId, Fun &undo,
                                            Fun &redo)
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
    return res;
}
bool ProjectItemModel::requestAddBinSubClip(QString &id, int in, int out, const QMap<QString, QString> &zoneProperties, const QString &parentId)
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
        Q_EMIT dataChanged(index, index, {AbstractProjectItem::DataName});
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

bool ProjectItemModel::requestCleanupUnused()
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::vector<std::shared_ptr<AbstractProjectItem>> to_delete;
    // Iterate to find clips that are not in timeline
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::ClipItem && !c->isIncludedInTimeline() && c->clipType() != ClipType::Timeline) {
            to_delete.push_back(c);
        }
    }
    // it is important to execute deletion in a separate loop, because otherwise
    // the iterators of m_allItems get messed up
    for (const auto &c : to_delete) {
        bool res = requestBinClipDeletion(c, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    pCore->pushUndo(undo, redo, i18n("Clean Project"));
    return true;
}

bool ProjectItemModel::requestTrashClips(QStringList &ids, QStringList &urls)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::vector<std::shared_ptr<AbstractProjectItem>> to_delete;
    // Iterate to find clips that are not in timeline
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (ids.contains(c->clipId())) {
            to_delete.push_back(c);
        }
    }

    // it is important to execute deletion in a separate loop, because otherwise
    // the iterators of m_allItems get messed up
    for (const auto &c : to_delete) {
        bool res = requestBinClipDeletion(c, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    for (const auto &url : urls) {
        QFile::remove(url);
    }
    // don't push undo/redo: the files are deleted we can't redo/undo
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

void ProjectItemModel::updateCacheThumbnail(std::unordered_map<QString, std::vector<int>> &thumbData)
{
    READ_LOCK();
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::ClipItem) {
            int frameNumber = qMax(0, std::static_pointer_cast<ProjectClip>(c)->getThumbFrame());
            thumbData[c->clipId()].push_back(frameNumber);
        } else if (c->itemType() == AbstractProjectItem::SubClipItem) {
            QPoint p = c->zone();
            thumbData[std::static_pointer_cast<ProjectSubClip>(c)->getMasterClip()->clipId()].push_back(p.x());
        }
    }
}

QStringList ProjectItemModel::getClipByUrl(const QFileInfo &url) const
{
    READ_LOCK();
    QStringList result;
    if (url.filePath().isEmpty()) {
        // Invalid url
        return result;
    }
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
    }

    // In case there are some non-existent parent, we fall back to root
    for (const auto &f : downLinks) {
        if (upLinks.count(f.first) == 0) {
            upLinks[f.first] = -1;
        }
        if (f.first != -1 && downLinks.count(upLinks[f.first]) == 0) {
            qWarning() << f.first << "has invalid parent folder" << upLinks[f.first] << "it will be placed in top directory";
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
    return !m_allIds.contains(id.toInt());
}

QList<QUuid> ProjectItemModel::loadBinPlaylist(Mlt::Service *documentTractor, std::unordered_map<QString, QString> &binIdCorresp, QStringList &expandedFolders,
                                               const QUuid &activeUuid, int &zoomLevel)
{
    QWriteLocker locker(&m_lock);
    clean();
    QList<QUuid> brokenSequences;
    QList<QUuid> foundSequences;
    QList<int> foundIds;
    Mlt::Properties retainList(mlt_properties(documentTractor->get_data("xml_retain")));
    if (retainList.is_valid()) {
        Mlt::Playlist playlist(mlt_playlist(retainList.get_data(BinPlaylist::binPlaylistId.toUtf8().constData())));
        if (playlist.is_valid() && playlist.type() == mlt_service_playlist_type) {
            // Load folders
            Mlt::Properties folderProperties;
            Mlt::Properties playlistProps(playlist.get_properties());
            expandedFolders = QString(playlistProps.get("kdenlive:expandedFolders")).split(QLatin1Char(';'));
            folderProperties.pass_values(playlistProps, "kdenlive:folder.");
            loadFolders(folderProperties, binIdCorresp);
            m_sequenceFolderId = -1;
            if (playlistProps.property_exists("kdenlive:sequenceFolder")) {
                int sequenceFolder = playlistProps.get_int("kdenlive:sequenceFolder");
                if (sequenceFolder > -1 && binIdCorresp.count(QString::number(sequenceFolder)) > 0) {
                    setSequencesFolder(binIdCorresp.at(QString::number(sequenceFolder)).toInt());
                }
            }

            m_audioCaptureFolderId = -1;
            if (playlistProps.property_exists("kdenlive:audioCaptureFolder")) {
                int audioCaptureFolder = playlistProps.get_int("kdenlive:audioCaptureFolder");
                if (audioCaptureFolder > -1 && binIdCorresp.count(QString::number(audioCaptureFolder)) > 0) {
                    setAudioCaptureFolder(binIdCorresp.at(QString::number(audioCaptureFolder)).toInt());
                }
            }

            // Load Zoom level
            if (playlistProps.property_exists("kdenlive:binZoom")) {
                zoomLevel = playlistProps.get_int("kdenlive:binZoom");
            }

            // Read notes
            QString notes = playlistProps.get("kdenlive:documentnotes");
            pCore->projectManager()->setDocumentNotes(notes);

            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            int max = playlist.count();
            if (max > 0) {
                Q_EMIT pCore->loadingMessageNewStage(i18n("Reading project clips…"), max);
            }
            QMap<int, std::shared_ptr<Mlt::Producer>> binProducers;
            for (int i = 0; i < max; i++) {
                Q_EMIT pCore->loadingMessageIncrease();
                qApp->processEvents();
                QScopedPointer<Mlt::Producer> prod(playlist.get_clip(i));
                if (prod->is_blank() || !prod->is_valid() || prod->parent().property_exists("kdenlive:remove")) {
                    qDebug() << "==== IGNORING BIN PRODUCER: " << prod->parent().get("kdenlive:id");
                    continue;
                }
                std::shared_ptr<Mlt::Producer> producer;
                if (prod->parent().property_exists("kdenlive:uuid")) {
                    const QUuid uuid(prod->parent().get("kdenlive:uuid"));
                    if (foundSequences.contains(uuid)) {
                        qDebug() << "FOUND DUPLICATED SEQUENCE: " << uuid.toString();
                        Q_ASSERT(false);
                    }
                    if (prod->parent().type() == mlt_service_tractor_type) {
                        // Load sequence properties
                        foundSequences << uuid;
                        Mlt::Properties sequenceProps;
                        sequenceProps.pass_values(prod->parent(), "kdenlive:sequenceproperties.");
                        pCore->currentDoc()->loadSequenceProperties(uuid, sequenceProps);

                        std::shared_ptr<Mlt::Tractor> trac = std::make_shared<Mlt::Tractor>(prod->parent());
                        int id(prod->parent().get_int("kdenlive:id"));
                        trac->set("kdenlive:id", id);
                        trac->set("kdenlive:uuid", uuid.toString().toUtf8().constData());
                        trac->set("length", prod->parent().get("length"));
                        trac->set("out", prod->parent().get("out"));
                        trac->set("kdenlive:clipname", prod->parent().get("kdenlive:clipname"));
                        trac->set("kdenlive:description", prod->parent().get("kdenlive:description"));
                        trac->set("kdenlive:folderid", prod->parent().get("kdenlive:folderid"));
                        trac->set("kdenlive:duration", prod->parent().get("kdenlive:duration"));
                        trac->set("kdenlive:producer_type", ClipType::Timeline);
                        trac->set("kdenlive:maxduration", prod->parent().get_int("kdenlive:maxduration"));
                        trac->set("_kdenlive_processed", 1);
                        std::shared_ptr<Mlt::Producer> prod2(trac->cut());

                        prod2->set("kdenlive:id", id);
                        prod2->set("kdenlive:uuid", uuid.toString().toUtf8().constData());
                        prod2->set("length", prod->parent().get("length"));
                        prod2->set("out", prod->parent().get("out"));
                        prod2->set("kdenlive:clipname", prod->parent().get("kdenlive:clipname"));
                        prod2->set("kdenlive:description", prod->parent().get("kdenlive:description"));
                        prod2->set("kdenlive:folderid", prod->parent().get("kdenlive:folderid"));
                        prod2->set("kdenlive:duration", prod->parent().get("kdenlive:duration"));
                        prod2->set("kdenlive:producer_type", ClipType::Timeline);
                        prod2->set("kdenlive:maxduration", prod->parent().get_int("kdenlive:maxduration"));
                        prod2->set("_kdenlive_processed", 1);
                        if (foundIds.contains(id)) {
                            qWarning() << "ERROR, several Sequence Clips using the same id: " << id << ", UUID: " << uuid.toString()
                                       << "\n____________________";
                            brokenSequences << uuid;
                        }
                        foundIds << id;
                        binProducers.insert(id, prod2);
                    } else {
                        const QString resource = prod->parent().get("resource");
                        qDebug() << "/// INCORRECT SEQUENCE FOUND IN PROJECT BIN: " << resource;
                        if (resource.endsWith(QLatin1String("<tractor>"))) {
                            // Buggy internal xml producer, drop
                            qDebug() << "/// AARGH INCORRECT SEQUENCE CLIP IN PROJECT BIN... TRY TO RECOVER";
                            brokenSequences.append(QUuid(uuid));
                        }
                    }
                    continue;
                }
                producer.reset(new Mlt::Producer(prod->parent()));
                int id = producer->get_int("kdenlive:id");
                if (!id) {
                    qDebug() << "WARNING THIS SHOULD NOT HAPPEN, BIN CLIP WITHOUT ID: " << producer->parent().get("resource")
                             << ", ID: " << producer->parent().get("id") << "\n\nFZFZFZFZFZFZFZFZFZFZFZFZFZ";
                    // Using a temporary negative reference so we don't mess with yet unloaded clips
                    id = -getFreeClipId();
                } else {
                    if (foundIds.contains(id)) {
                        qWarning() << "ERROR, several Bin Clips using the same id: " << id << "\n____________________";
                        Q_ASSERT(false);
                    }
                    foundIds << id;
                }
                binProducers.insert(id, producer);
            }
            // Ensure active playlist is in the project bin
            if (!foundSequences.contains(activeUuid)) {
                // Project corruption, try to restore sequence from the ProjectTractor
                Mlt::Tractor tractor(*documentTractor);
                std::shared_ptr<Mlt::Producer> tk(tractor.track(0));
                std::shared_ptr<Mlt::Producer> producer(new Mlt::Producer(tk->parent()));
                const QUuid uuid(producer->get("kdenlive:uuid"));
                if (!uuid.isNull() && uuid == activeUuid) {
                    // TODO: Show user info about the recovered corruption
                    qWarning() << "Recovering corrupted sequence: " << uuid.toString();
                    int id = producer->get_int("kdenlive:id");
                    binProducers.insert(id, std::move(producer));
                    foundSequences << activeUuid;
                } else {
                    brokenSequences << activeUuid;
                }
            }
            // Do the real insertion
            QList<int> binIds = binProducers.keys();

            if (binIds.length() > 0) {
                Q_EMIT pCore->loadingMessageNewStage(i18n("Loading project clips…"), binIds.length());
            }

            while (!binProducers.isEmpty()) {
                Q_EMIT pCore->loadingMessageIncrease();
                qApp->processEvents();
                int bid = binIds.takeFirst();
                std::shared_ptr<Mlt::Producer> prod = binProducers.take(bid);
                QString newId = QString::number(getFreeClipId());
                QString parentId = qstrdup(prod->get("kdenlive:folderid"));
                if (parentId.isEmpty()) {
                    parentId = QStringLiteral("-1");
                } else {
                    if (binIdCorresp.count(parentId.section(QLatin1Char('.'), -1)) == 0) {
                        // Error, folder was lost
                        parentId = QStringLiteral("-1");
                    }
                }
                prod->set("_kdenlive_processed", 1);
                requestAddBinClip(newId, prod, parentId, undo, redo);
                qApp->processEvents();
                binIdCorresp[QString::number(bid)] = newId;
            }
        }
    } else {
        qDebug() << "HHHHHHHHHHHH\nINVALID BIN PLAYLIST...";
    }
    return brokenSequences;
}

void ProjectItemModel::loadTractorPlaylist(Mlt::Tractor documentTractor, std::unordered_map<QString, QString> &binIdCorresp)
{
    QWriteLocker locker(&m_lock);
    QList<int> processedIds;
    QMap<int, std::shared_ptr<Mlt::Producer>> binProducers;
    for (int i = 1; i < documentTractor.count(); i++) {
        std::unique_ptr<Mlt::Producer> track(documentTractor.track(i));
        Mlt::Service service(track->get_service());
        Mlt::Tractor tractor(service);
        if (tractor.count() < 2) {
            qDebug() << "// TRACTOR PROBLEM";
            return;
        }
        for (int j = 0; j < tractor.count(); j++) {
            std::unique_ptr<Mlt::Producer> trackProducer(tractor.track(j));
            Mlt::Playlist trackPlaylist(mlt_playlist(trackProducer->get_service()));
            for (int k = 0; k < trackPlaylist.count(); k++) {
                if (trackPlaylist.is_blank(k)) {
                    continue;
                }
                std::unique_ptr<Mlt::Producer> clip(trackPlaylist.get_clip(k));
                if (clip->parent().property_exists("kdenlive:id")) {
                    int cid = clip->parent().get_int("kdenlive:id");
                    if (processedIds.contains(cid)) {
                        continue;
                    }
                    const QString service = clip->parent().get("mlt_service");
                    const QString resource = clip->parent().get("resource");
                    const QString hash = clip->parent().get("kdenlive:file_hash");
                    ClipType::ProducerType matchType;
                    if (service.startsWith(QLatin1String("avformat"))) {
                        int cType = clip->parent().get_int("kdenlive:clip_type");
                        switch (cType) {
                        case 1:
                            matchType = ClipType::Audio;
                            break;
                        case 2:
                            matchType = ClipType::Video;
                            break;
                        default:
                            matchType = ClipType::AV;
                            break;
                        }
                    } else {
                        matchType = ClipLoadTask::getTypeForService(service, resource);
                    }
                    qDebug() << ":::: LOOKING FOR A MATCHING ITEM TYPE: " << matchType;
                    // Try to find matching clip
                    bool found = false;
                    for (const auto &clip : m_allItems) {
                        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
                        if (c->itemType() == AbstractProjectItem::ClipItem) {
                            if (c->clipType() == matchType) {
                                std::shared_ptr<ProjectClip> pClip = std::static_pointer_cast<ProjectClip>(c);
                                qDebug() << "::::::\nTRYING TO MATCH: " << pClip->getProducerProperty(QStringLiteral("resource")) << " = " << resource;
                                if (pClip->getProducerProperty(QStringLiteral("kdenlive:file_hash")) == hash) {
                                    // Found a match
                                    binIdCorresp[QString::number(cid)] = pClip->clipId();
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!found) {
                        std::shared_ptr<Mlt::Producer> clipProd(new Mlt::Producer(clip->parent()));
                        binProducers.insert(cid, clipProd);
                    }
                    processedIds << cid;
                }
                qDebug() << ":::: FOUND CLIPS IN PLAYLIST: " << i << ": " << trackPlaylist.count();
            }
        }
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // Do the real insertion
    QList<int> binIds = binProducers.keys();
    while (!binProducers.isEmpty()) {
        int bid = binIds.takeFirst();
        std::shared_ptr<Mlt::Producer> prod = binProducers.take(bid);
        QString newId = QString::number(getFreeClipId());
        QString parentId = qstrdup(prod->get("kdenlive:folderid"));
        if (parentId.isEmpty()) {
            parentId = QStringLiteral("-1");
        } else {
            if (binIdCorresp.count(parentId.section(QLatin1Char('.'), -1)) == 0) {
                // Error, folder was lost
                parentId = QStringLiteral("-1");
            }
        }
        prod->set("_kdenlive_processed", 1);
        qDebug() << "======\nADDING NEW CLIP: " << newId << " = " << prod->is_valid();
        requestAddBinClip(newId, prod, parentId, undo, redo);
        binIdCorresp[QString::number(bid)] = newId;
    }
}

void ProjectItemModel::storeSequence(const QString uuid, std::shared_ptr<Mlt::Tractor> tractor, bool internalSave)
{
    if (m_extraPlaylists.count(uuid) > 0) {
        m_extraPlaylists.erase(uuid);
    }
    m_extraPlaylists.insert({uuid, std::move(tractor)});
    if (internalSave) {
        // Ensure we never use the mapped ids when re-opening an already opened sequence
        setExtraTimelineSaved(uuid);
    }
}

int ProjectItemModel::sequenceCount() const
{
    return m_extraPlaylists.size();
}

std::shared_ptr<Mlt::Tractor> ProjectItemModel::projectTractor()
{
    return m_projectTractor;
}

const QString ProjectItemModel::sceneList(const QString &root, const QString &fullPath, const QString &filterData, Mlt::Tractor *activeTractor, int duration)
{
    QMutexLocker lock(&pCore->xmlMutex);
    LocaleHandling::resetLocale();
    QString playlist;
    Mlt::Consumer xmlConsumer(pCore->getProjectProfile(), "xml", fullPath.isEmpty() ? "kdenlive_playlist" : fullPath.toUtf8().constData());
    if (!root.isEmpty()) {
        xmlConsumer.set("root", root.toUtf8().constData());
    }
    if (!xmlConsumer.is_valid()) {
        return QString();
    }
    xmlConsumer.set("store", "kdenlive");
    xmlConsumer.set("time_format", "clock");
    // Disabling meta creates cleaner files, but then we don't have access to metadata on the fly (meta channels, etc)
    // And we must use "avformat" instead of "avformat-novalidate" on project loading which causes a big delay on project opening
    // xmlConsumer.set("no_meta", 1);
    // Add active timeline as playlist of the main tractor so that when played through melt, the .kdenlive file reads the playlist
    if (m_projectTractor->count() > 0) {
        m_projectTractor->remove_track(0);
    }
    std::unique_ptr<Mlt::Producer> cut(activeTractor->cut(0, duration));
    m_projectTractor->insert_track(*cut.get(), 0);

    Mlt::Service s(m_projectTractor->get_service());
    std::unique_ptr<Mlt::Filter> filter = nullptr;
    if (!filterData.isEmpty()) {
        filter = std::make_unique<Mlt::Filter>(pCore->getProjectProfile(), QString("dynamictext:%1").arg(filterData).toUtf8().constData());
        filter->set("fgcolour", "#ffffff");
        filter->set("bgcolour", "#bb333333");
        s.attach(*filter.get());
    }
    xmlConsumer.connect(s);
    xmlConsumer.run();
    if (filter) {
        s.detach(*filter.get());
    }
    playlist = fullPath.isEmpty() ? QString::fromUtf8(xmlConsumer.get("kdenlive_playlist")) : fullPath;
    return playlist;
}

std::shared_ptr<Mlt::Tractor> ProjectItemModel::getExtraTimeline(const QString &uuid)
{
    if (m_extraPlaylists.count(uuid) > 0) {
        return m_extraPlaylists.at(uuid);
    }
    return nullptr;
}

void ProjectItemModel::setExtraTimelineSaved(const QString &uuid)
{
    if (m_extraPlaylists.count(uuid) > 0) {
        m_extraPlaylists.at(uuid)->set("_dontmapids", 1);
    }
}

void ProjectItemModel::removeReferencedClips(const QUuid &uuid, bool onDeletion)
{
    QList<std::shared_ptr<ProjectClip>> clipList = getRootFolder()->childClips();
    for (const std::shared_ptr<ProjectClip> &clip : qAsConst(clipList)) {
        if (clip->refCount() > 0) {
            clip->purgeReferences(uuid, onDeletion);
        }
    }
}

/** @brief Save document properties in MLT's bin playlist */
void ProjectItemModel::saveDocumentProperties(const QMap<QString, QString> &props, const QMap<QString, QString> &metadata)
{
    m_binPlaylist->saveDocumentProperties(props, metadata);
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
        clip->setClipStatus(FileStatus::StatusWaiting);
    }
}

void ProjectItemModel::setClipInvalid(const QString &binId)
{
    QWriteLocker locker(&m_lock);
    std::shared_ptr<ProjectClip> clip = getClipByBinID(binId);
    if (clip) {
        clip->setClipStatus(FileStatus::StatusMissing);
        // TODO: set producer as blank invalid
    }
}

void ProjectItemModel::slotUpdateInvalidCount()
{
    READ_LOCK();
    int missingCount = 0;
    int missingUsed = 0;
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->clipStatus() == FileStatus::StatusMissing) {
            int usage = c->getData(AbstractProjectItem::UsageCount).toInt();
            if (usage > 0) {
                missingUsed++;
            }
            missingCount++;
        }
    }
    Q_EMIT pCore->gotMissingClipsCount(missingCount, missingUsed);
}

void ProjectItemModel::updateWatcher(const std::shared_ptr<ProjectClip> &clipItem)
{
    QWriteLocker locker(&m_lock);
    ClipType::ProducerType type = clipItem->clipType();
    if (type == ClipType::AV || type == ClipType::Audio || type == ClipType::Image || type == ClipType::Video || type == ClipType::Playlist ||
        type == ClipType::TextTemplate || type == ClipType::Animation) {
        m_fileWatcher->removeFile(clipItem->clipId());
        QFileInfo check_file(clipItem->clipUrl());
        // check if file exists and if yes: Is it really a file and no directory?
        if ((check_file.exists() && check_file.isFile()) || clipItem->clipStatus() == FileStatus::StatusMissing) {
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

bool ProjectItemModel::hasProxies() const
{
    READ_LOCK();
    for (const auto &clip : m_allItems) {
        auto c = std::static_pointer_cast<AbstractProjectItem>(clip.second.lock());
        if (c->itemType() == AbstractProjectItem::ClipItem) {
            if (std::static_pointer_cast<ProjectClip>(c)->hasProxy()) {
                return true;
            }
        }
    }
    return false;
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

bool ProjectItemModel::urlExists(const QString &path) const
{
    return m_fileWatcher->contains(path);
}

bool ProjectItemModel::canBeEmbeded(const QUuid destUuid, const QUuid srcUuid)
{
    const QString srcId = getSequenceId(destUuid);
    QList<QUuid> checkedUuids;
    std::shared_ptr<ProjectClip> clip = getClipByBinID(srcId);
    if (clip) {
        // Get a list of all dependencies
        QList<QUuid> updatedUUids = clip->registeredUuids();
        while (!updatedUUids.isEmpty()) {
            QUuid uuid = updatedUUids.takeFirst();
            if (!checkedUuids.contains(uuid)) {
                checkedUuids.append(uuid);
                const QString secId = getSequenceId(uuid);
                std::shared_ptr<ProjectClip> subclip = getClipByBinID(secId);
                updatedUUids.append(subclip->registeredUuids());
            }
        }
        if (checkedUuids.contains(srcUuid)) {
            return false;
        }
    } else {
        qDebug() << "::: CLIP NOT FOUND FOR : " << srcId;
    }
    return true;
}

int ProjectItemModel::defaultSequencesFolder() const
{
    return m_sequenceFolderId;
}

void ProjectItemModel::setSequencesFolder(int id)
{
    m_sequenceFolderId = id;
    saveProperty(QStringLiteral("kdenlive:sequenceFolder"), QString::number(id));
    if (id > -1) {
        Q_ASSERT(getFolderByBinId(QString::number(id)) != nullptr);
        onItemUpdated(QString::number(id), Qt::DecorationRole);
    }
}

int ProjectItemModel::defaultAudioCaptureFolder() const
{
    return m_audioCaptureFolderId;
}

void ProjectItemModel::setAudioCaptureFolder(int id)
{
    m_audioCaptureFolderId = id;
    saveProperty(QStringLiteral("kdenlive:audioCaptureFolder"), QString::number(id));
}

Fun ProjectItemModel::removeProjectItem_lambda(int binId, int id)
{
    return [this, binId, id]() {
        if (binId > -1) {
            Q_EMIT pCore->binClipDeleted(binId);
        }
        Fun operation = removeItem_lambda(id);
        bool result = operation();
        return result;
    };
}

void ProjectItemModel::checkSequenceIntegrity(const QString activeSequenceId)
{
    QStringList sequencesIds = pCore->currentDoc()->getTimelinesIds();
    Q_ASSERT(sequencesIds.contains(activeSequenceId));
    QStringList allMltIds = m_binPlaylist->getAllMltIds();
    for (auto &i : sequencesIds) {
        Q_ASSERT(allMltIds.contains(i));
    }
}

std::shared_ptr<EffectStackModel> ProjectItemModel::getClipEffectStack(int itemId)
{
    std::shared_ptr<ProjectClip> clip = getClipByBinID(QString::number(itemId));
    Q_ASSERT(clip != nullptr);
    return clip->getEffectStack();
}
