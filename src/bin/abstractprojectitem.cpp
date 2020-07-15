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

#include "abstractprojectitem.h"
#include "bin.h"
#include "core.h"
#include "jobs/jobmanager.h"
#include "macros.hpp"
#include "projectitemmodel.h"

#include "jobs/audiothumbjob.hpp"
#include "jobs/loadjob.hpp"
#include "jobs/thumbjob.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QVariant>
#include <utility>
AbstractProjectItem::AbstractProjectItem(PROJECTITEMTYPE type, QString id, const std::shared_ptr<ProjectItemModel> &model, bool isRoot)
    : TreeItem(QList<QVariant>(), std::static_pointer_cast<AbstractTreeModel>(model), isRoot)
    , m_name()
    , m_description()
    , m_thumbnail(QIcon())
    , m_date()
    , m_binId(std::move(id))
    , m_usage(0)
    , m_rating(0)
    , m_clipStatus(StatusReady)
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

void AbstractProjectItem::setRefCount(uint count)
{
    m_usage = count;
    if (auto ptr = m_model.lock())
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<AbstractProjectItem>(shared_from_this()),
                                                                       AbstractProjectItem::UsageCount);
}

uint AbstractProjectItem::refCount() const
{
    return m_usage;
}

void AbstractProjectItem::addRef()
{
    m_usage++;
    if (auto ptr = m_model.lock())
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<AbstractProjectItem>(shared_from_this()),
                                                                       AbstractProjectItem::UsageCount);
}

void AbstractProjectItem::removeRef()
{
    m_usage--;
    if (auto ptr = m_model.lock())
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<AbstractProjectItem>(shared_from_this()),
                                                                       AbstractProjectItem::UsageCount);
}

const QString &AbstractProjectItem::clipId() const
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

QVariant AbstractProjectItem::getData(DataType type) const
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
        data = QVariant(m_binId);
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
        data = QVariant(m_usage);
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
    case JobType:
        if (itemType() == ClipItem) {
            auto jobIds = pCore->jobManager()->getPendingJobsIds(clipId());
            if (jobIds.empty()) {
                jobIds = pCore->jobManager()->getFinishedJobsIds(clipId());
            }
            if (jobIds.size() > 0) {
                data = QVariant(pCore->jobManager()->getJobType(jobIds[0]));
            }
        }
        break;
    case JobStatus:
        if (itemType() == ClipItem) {
            auto jobIds = pCore->jobManager()->getPendingJobsIds(clipId());
            if (jobIds.empty()) {
                jobIds = pCore->jobManager()->getFinishedJobsIds(clipId());
            }
            if (jobIds.size() > 0) {
                data = QVariant::fromValue(pCore->jobManager()->getJobStatus(jobIds[0]));
            } else {
                data = QVariant::fromValue(JobManagerStatus::NoJob);
            }
        }
        break;
    case JobProgress:
        if (itemType() == ClipItem) {
            auto jobIds = pCore->jobManager()->getPendingJobsIds(clipId());
            if (jobIds.size() > 0) {
                data = QVariant(pCore->jobManager()->getJobProgressForClip(jobIds[0], clipId()));
            } else {
                data = QVariant(0);
            }
        }
        break;
    case JobSuccess:
        if (itemType() == ClipItem) {
            auto jobIds = pCore->jobManager()->getFinishedJobsIds(clipId());
            if (jobIds.size() > 0) {
                // Check the last job status
                data = QVariant(pCore->jobManager()->jobSucceded(jobIds[jobIds.size() - 1]));
            } else {
                data = QVariant(true);
            }
        }
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
    return 8;
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

void AbstractProjectItem::setClipStatus(CLIPSTATUS status)
{
    m_clipStatus = status;
}

bool AbstractProjectItem::statusReady() const
{
    return m_clipStatus == StatusReady || m_clipStatus == StatusProxyOnly;
}

AbstractProjectItem::CLIPSTATUS AbstractProjectItem::clipStatus() const
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
    pCore->jobManager()->slotDiscardClipJobs(clipId());
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

const QString & AbstractProjectItem::tags() const
{
    return m_tags;
}

void AbstractProjectItem::setTags(const QString tags)
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
