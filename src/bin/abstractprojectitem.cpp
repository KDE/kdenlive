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

#include <QDomElement>
#include <QVariant>
#include <QPainter>

AbstractProjectItem::AbstractProjectItem(PROJECTITEMTYPE type, const QString &id, AbstractProjectItem *parent) :
    QObject()
    , m_parent(parent)
    , m_id(id)
    , m_usage(0)
    , m_clipStatus(StatusReady)
    , m_jobType(AbstractClipJob::NOJOBTYPE)
    , m_jobProgress(0)
    , m_itemType(type)
    , m_isCurrent(false)
{
}

AbstractProjectItem::AbstractProjectItem(PROJECTITEMTYPE type, const QDomElement &description, AbstractProjectItem *parent) :
    QObject()
    , m_parent(parent)
    , m_id(description.attribute(QStringLiteral("id")))
    , m_usage(0)
    , m_clipStatus(StatusReady)
    , m_jobType(AbstractClipJob::NOJOBTYPE)
    , m_jobProgress(0)
    , m_itemType(type)
    , m_isCurrent(false)
{
}

AbstractProjectItem::~AbstractProjectItem()
{
    while (!isEmpty()) {
        AbstractProjectItem *child = takeFirst();
        removeChild(child);
        delete child;
    }
}

bool AbstractProjectItem::operator==(const AbstractProjectItem *projectItem) const
{
    // FIXME: only works for folders
    bool equal = static_cast<const QList *const>(this) == static_cast<const QList *const>(projectItem);
    equal &= m_parent == projectItem->parent();
    return equal;
}

AbstractProjectItem *AbstractProjectItem::parent() const
{
    return m_parent;
}

void AbstractProjectItem::setRefCount(uint count)
{
    m_usage = count;
    bin()->emitItemUpdated(this);
}

uint AbstractProjectItem::refCount() const
{
    return m_usage;
}

void AbstractProjectItem::addRef()
{
    m_usage++;
    bin()->emitItemUpdated(this);
}

void AbstractProjectItem::removeRef()
{
    m_usage--;
    bin()->emitItemUpdated(this);
}

const QString &AbstractProjectItem::clipId() const
{
    return m_id;
}

void AbstractProjectItem::setParent(AbstractProjectItem *parent)
{
    if (m_parent != parent) {
        if (m_parent) {
            m_parent->removeChild(this);
        }
        m_parent = parent;
        QObject::setParent(m_parent);
    }

    if (m_parent && !m_parent->contains(this)) {
        m_parent->addChild(this);
    }
}

Bin *AbstractProjectItem::bin()
{
    if (m_parent) {
        return m_parent->bin();
    }
    return nullptr;
}

QPixmap AbstractProjectItem::roundedPixmap(const QPixmap &source)
{
    QPixmap pix(source.width(), source.height());
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addRoundedRect(0.5, 0.5, pix.width() - 1, pix.height() - 1, 4, 4);
    p.setClipPath(path);
    p.drawPixmap(0, 0, source);
    p.end();
    return pix;
}

void AbstractProjectItem::addChild(AbstractProjectItem *child)
{
    if (child && !contains(child)) {
        bin()->emitAboutToAddItem(child);
        append(child);
        bin()->emitItemAdded(child);
    }
}

void AbstractProjectItem::removeChild(AbstractProjectItem *child)
{
    if (child && contains(child)) {
        bin()->emitAboutToRemoveItem(child);
        removeAll(child);
        bin()->emitItemRemoved(child);
    }
}

int AbstractProjectItem::index() const
{
    if (m_parent) {
        return m_parent->indexOf(const_cast<AbstractProjectItem *>(this));
    }
    return 0;
}

AbstractProjectItem::PROJECTITEMTYPE AbstractProjectItem::itemType() const
{
    return m_itemType;
}

QVariant AbstractProjectItem::data(DataType type) const
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
        data = QVariant(m_id);
        break;
    case DataDuration:
        data = QVariant(m_duration);
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
    case JobType:
        data = QVariant(m_jobType);
        break;
    case JobProgress:
        data = QVariant(m_jobProgress);
        break;
    case JobMessage:
        data = QVariant(m_jobMessage);
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
    return 3;
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
    return QPoint();
}

void AbstractProjectItem::setClipStatus(CLIPSTATUS status)
{
    m_clipStatus = status;
}

bool AbstractProjectItem::statusReady() const
{
    return m_clipStatus == StatusReady;
}

AbstractProjectItem::CLIPSTATUS AbstractProjectItem::clipStatus() const
{
    return m_clipStatus;
}

