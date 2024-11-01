/*
SPDX-FileCopyrightText: 2012 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "abstractprojectitem.h"
#include "bin.h"
#include "core.h"
#include "macros.hpp"
#include "projectitemmodel.h"

#include <QPainter>
#include <QPainterPath>
#include <QVariant>

#include <KLocalizedString>
#include <utility>

AbstractProjectItem::AbstractProjectItem(PROJECTITEMTYPE type, QString id, const std::shared_ptr<ProjectItemModel> &model, bool isRoot)
    : TreeItem(QList<QVariant>(), std::static_pointer_cast<AbstractTreeModel>(model), isRoot)
    , m_name()
    , m_description()
    , m_thumbnail(QIcon())
    , m_parentDuration()
    , m_inPoint()
    , m_outPoint()
    , m_date()
    , m_binId(std::move(id))
    , m_totalUsage(0)
    , m_currentSequenceUsage(0)
    , m_AudioUsage(0)
    , m_VideoUsage(0)
    , m_rating(0)
    , m_clipStatus(FileStatus::StatusReady)
    , m_itemType(type)
    , m_lock(QReadWriteLock::Recursive)
    , m_isCurrent(false)
{
    Q_ASSERT(!isRoot || type == FolderItem);
}

bool AbstractProjectItem::operator==(const std::shared_ptr<AbstractProjectItem> &projectItem) const
{
    // FIXME: only works for folders
    bool equal = this->m_childItems == projectItem->m_childItems;
    // equal = equal && (m_parentItem == projectItem->m_parentItem);
    return equal;
}

std::shared_ptr<AbstractProjectItem> AbstractProjectItem::parent() const
{
    return std::static_pointer_cast<AbstractProjectItem>(m_parentItem.lock());
}

void AbstractProjectItem::setRefCount(uint sequenceCount, uint totalCount)
{
    if (sequenceCount == totalCount) {
        if (sequenceCount == 0) {
            m_usageText.clear();
        } else {
            m_usageText = QString::number(sequenceCount);
        }
    } else {
        m_usageText = QStringLiteral("%1|%2").arg(sequenceCount).arg(totalCount);
    }
    m_currentSequenceUsage = sequenceCount;
    m_totalUsage = totalCount;
    m_VideoUsage = totalCount - m_AudioUsage;
    if (auto ptr = m_model.lock())
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(
            std::static_pointer_cast<AbstractProjectItem>(shared_from_this()),
            {AbstractProjectItem::UsageCount, AbstractProjectItem::VideoUsed, AbstractProjectItem::AudioUsed});
}

uint AbstractProjectItem::refCount() const
{
    return m_totalUsage;
}

void AbstractProjectItem::addRef()
{
    m_currentSequenceUsage++;
    m_totalUsage++;
    if (auto ptr = m_model.lock())
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<AbstractProjectItem>(shared_from_this()),
                                                                       {AbstractProjectItem::UsageCount});
}

void AbstractProjectItem::removeRef()
{
    m_currentSequenceUsage--;
    m_totalUsage--;
    if (auto ptr = m_model.lock())
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<AbstractProjectItem>(shared_from_this()),
                                                                       {AbstractProjectItem::UsageCount});
}

const QString &AbstractProjectItem::clipId(bool) const
{
    return m_binId;
}

QPixmap AbstractProjectItem::roundedPixmap(const QPixmap &source)
{
    QPixmap pix(source.size());
    pix.fill(QColor(0, 0, 0, 100));
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(0.5, 0.5, pix.width() - 1, pix.height() - 1, 4, 4);
    p.setClipPath(path);
    p.drawPixmap(0, 0, source);
    p.end();
    return pix;
}

AbstractProjectItem::PROJECTITEMTYPE AbstractProjectItem::itemType() const
{
    return m_itemType;
}

const QIcon AbstractProjectItem::icon() const
{
    return m_thumbnail;
}

const QVariant AbstractProjectItem::getData(DataType type) const
{
    QVariant data;
    switch (type) {
    case DataName:
        data = QVariant(m_name);
        break;
    case DataDescription:
        data = QVariant(m_description);
        break;
    case DataThumbnail:
        data = QVariant(m_thumbnail);
        break;
    case DataId:
        data = QVariant(m_binId.toInt());
        break;
    case DataDuration:
        data = QVariant(m_duration);
        break;
    case ParentDuration:
        data = QVariant(m_parentDuration);
        break;
    case DataInPoint:
        data = QVariant(m_inPoint);
        break;
    case DataOutPoint:
        data = QVariant(m_outPoint);
        break;
    case DataDate:
        data = QVariant(m_date);
        break;
    case UsageCount:
        data = QVariant(m_usageText);
        break;
    case AudioUsed:
        data = QVariant(m_AudioUsage > 0);
        break;
    case VideoUsed:
        data = QVariant(m_VideoUsage > 0);
        break;
    case ItemTypeRole:
        data = QVariant(m_itemType);
        break;
    case ClipType:
        data = clipType();
        break;
    case DataTag:
        data = QVariant(m_tags);
        break;
    case DataRating:
        data = QVariant(m_rating);
        break;
    case ClipHasAudioAndVideo:
        data = hasAudioAndVideo();
        break;
    case JobStatus:
        if (itemType() == ClipItem) {
            data = QVariant::fromValue(pCore->taskManager.jobStatus(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid())));
            /*
            auto jobIds = pCore->jobManager()->getPendingJobsIds(clipId());
            if (jobIds.empty()) {
                jobIds = pCore->jobManager()->getFinishedJobsIds(clipId());
            }
            if (jobIds.size() > 0) {
                data = QVariant::fromValue(pCore->jobManager()->getJobStatus(jobIds[0]));
            } else {
                data = QVariant::fromValue(JobManagerStatus::NoJob);
            }*/
        }
        break;
    case JobProgress:
        if (itemType() == ClipItem) {
            return m_jobsProgress;
        }
        break;
    case JobSuccess:
        // TODO: reimplement ?
        /*if (itemType() == ClipItem) {
            auto jobIds = pCore->jobManager()->getFinishedJobsIds(clipId());
            if (jobIds.size() > 0) {
                // Check the last job status
                data = QVariant(pCore->jobManager()->jobSucceded(jobIds[jobIds.size() - 1]));
            } else {

            }
        }*/
        data = QVariant(true);
        break;
    case SequenceFolder:
        if (itemType() == FolderItem) {
            if (auto ptr = m_model.lock()) {
                if (m_binId.toInt() == std::static_pointer_cast<ProjectItemModel>(ptr)->defaultSequencesFolder()) {
                    data = QVariant(true);
                    break;
                }
            }
        }
        data = QVariant(false);
        break;
    case ClipStatus:
        data = QVariant(m_clipStatus);
        break;
    case ClipToolTip:
        data = QVariant(getToolTip());
        break;
    default:
        break;
    }
    return data;
}

int AbstractProjectItem::supportedDataCount() const
{
    return 9;
}

QString AbstractProjectItem::name() const
{
    return m_name;
}

void AbstractProjectItem::setName(const QString &name)
{
    m_name = name;
}

QString AbstractProjectItem::description() const
{
    return m_description;
}

void AbstractProjectItem::setDescription(const QString &description)
{
    m_description = description;
}

QPoint AbstractProjectItem::zone() const
{
    return {};
}

void AbstractProjectItem::setClipStatus(FileStatus::ClipStatus status)
{
    m_clipStatus = status;
}

bool AbstractProjectItem::statusReady() const
{
    return m_clipStatus == FileStatus::StatusReady || m_clipStatus == FileStatus::StatusProxy || m_clipStatus == FileStatus::StatusProxyOnly;
}

FileStatus::ClipStatus AbstractProjectItem::clipStatus() const
{
    return m_clipStatus;
}

std::shared_ptr<AbstractProjectItem> AbstractProjectItem::getEnclosingFolder(bool strict)
{
    if (!strict && itemType() == AbstractProjectItem::FolderItem) {
        return std::static_pointer_cast<AbstractProjectItem>(shared_from_this());
    }
    if (auto ptr = m_parentItem.lock()) {
        return std::static_pointer_cast<AbstractProjectItem>(ptr)->getEnclosingFolder(false);
    }
    return std::shared_ptr<AbstractProjectItem>();
}

bool AbstractProjectItem::selfSoftDelete(Fun &undo, Fun &redo)
{
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    for (const auto &child : m_childItems) {
        bool res = std::static_pointer_cast<AbstractProjectItem>(child)->selfSoftDelete(local_undo, local_redo);
        if (!res) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

QString AbstractProjectItem::lastParentId() const
{
    return m_lastParentId;
}

void AbstractProjectItem::updateParent(std::shared_ptr<TreeItem> newParent)
{
    // bool reload = !m_lastParentId.isEmpty();
    m_lastParentId.clear();
    if (newParent) {
        m_lastParentId = std::static_pointer_cast<AbstractProjectItem>(newParent)->clipId();
    }
    TreeItem::updateParent(newParent);
}

const QString &AbstractProjectItem::tags() const
{
    return m_tags;
}

void AbstractProjectItem::setTags(const QString &tags)
{
    m_tags = tags;
}

uint AbstractProjectItem::rating() const
{
    return m_rating;
}

void AbstractProjectItem::setRating(uint rating)
{
    m_rating = rating;
}

Fun AbstractProjectItem::getAudio_lambda()
{
    return []() {
        qDebug() << "============\n\nABSTRACT AUDIO CHECK\n\n===========";
        return true;
    };
}
