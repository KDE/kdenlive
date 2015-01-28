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
#include <QDebug>


AbstractProjectItem::AbstractProjectItem(PROJECTITEMTYPE type, const QString &id, AbstractProjectItem* parent) :
    QObject()
    , m_parent(NULL)
    , m_id(id)
    , m_isCurrent(false)
    , m_jobProgress(0)
    , m_jobType(AbstractClipJob::NOJOBTYPE)
    , m_itemType(type)
{
}

AbstractProjectItem::AbstractProjectItem(PROJECTITEMTYPE type, const QDomElement& description, AbstractProjectItem* parent) :
    QObject()
    , m_parent(NULL)
    , m_id(description.attribute("id"))
    , m_isCurrent(false)
    , m_jobProgress(0)
    , m_jobType(AbstractClipJob::NOJOBTYPE)
    , m_itemType(type)
{
}

AbstractProjectItem::~AbstractProjectItem()
{
}

bool AbstractProjectItem::operator==(const AbstractProjectItem* projectItem) const
{
    // FIXME: only works for folders
    bool equal = static_cast<const QList* const>(this) == static_cast<const QList* const>(projectItem);
    equal &= m_parent == projectItem->parent();
    return equal;
}

AbstractProjectItem* AbstractProjectItem::parent() const
{
    return m_parent;
}

const QString &AbstractProjectItem::clipId() const
{
    return m_id;
}


void AbstractProjectItem::setParent(AbstractProjectItem* parent)
{
    if (m_parent != parent) {
        if (m_parent) {
            m_parent->removeChild(this);
        }
        
        // Check if we are trying to delete the item
        if (m_isCurrent && parent == NULL) {
            /*if (bin()) {
                bin()->setCurrentItem(NULL);
            }*/
        }        
        m_parent = parent;
        QObject::setParent(m_parent);
    }

    if (m_parent && !m_parent->contains(this)) {
        m_parent->addChild(this);
    }
}

Bin* AbstractProjectItem::bin()
{
    if (m_parent) {
        return m_parent->bin();
    }
    return NULL;
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

void AbstractProjectItem::finishInsert(AbstractProjectItem* parent)
{
    /*if (m_parent && !m_parent->contains(this)) {
        m_parent->addChild(this);
    }*/
    //bin()->emitItemReady(this);
}

void AbstractProjectItem::addChild(AbstractProjectItem* child)
{
    if (child && !contains(child)) {
        bin()->emitAboutToAddItem(child);
        append(child);
	bin()->emitItemAdded(child);
    }
}

void AbstractProjectItem::removeChild(AbstractProjectItem* child)
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
        return m_parent->indexOf(const_cast<AbstractProjectItem*>(this));
    }

    return 0;
}

AbstractProjectItem::PROJECTITEMTYPE AbstractProjectItem::itemType() const
{
    return m_itemType;
}

bool AbstractProjectItem::rename(const QString &name)
{
    if (m_name == name) return false;
    QMap <QString, QString> newProperites;
    QMap <QString, QString> oldProperites;
    if (m_itemType == ClipItem) {
        // Rename clip
        oldProperites.insert("kdenlive:clipname", m_name);
        newProperites.insert("kdenlive:clipname", name);
        bin()->slotEditClipCommand(m_id, oldProperites, newProperites);
    }
    m_name = name;
    return true;
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
	case DataDuration:
	    data = QVariant(m_duration);
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
        case DataId:
            data = QVariant(m_id);
            break;
        default:
            break;
    }
    return data;
}

int AbstractProjectItem::supportedDataCount() const
{
    return 1;
}

QString AbstractProjectItem::name() const
{
    return m_name;
}

void AbstractProjectItem::setName(const QString& name)
{
    m_name = name;
}

QString AbstractProjectItem::description() const
{
    return m_description;
}

void AbstractProjectItem::setDescription(const QString& description)
{
    m_description = description;
}

void AbstractProjectItem::setCurrent(bool current, bool notify)
{
    if (m_isCurrent != current) 
    {
        m_isCurrent = current;
	if (!notify) 
	    return;
        /*if (current) {
            bin()->setCurrentItem(this);
        } else {
            bin()->setCurrentItem(NULL);
        }*/
    }
}

AbstractProjectItem* AbstractProjectItem::upFolder()
{
    for (int i = 0; i < count(); ++i) {
        AbstractProjectItem *child = at(i);
        if (child->itemType() == FolderUpItem) {
            return child;
        }
    }
    return NULL;
}

QPoint AbstractProjectItem::zone() const
{
    return QPoint();
}

void AbstractProjectItem::setClipStatus(CLIPSTATUS status)
{
    m_clipStatus = status;
}

