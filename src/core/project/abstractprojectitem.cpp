/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstractprojectitem.h"
#include "binmodel.h"
#include <QDomElement>
#include <QVariant>
#include <QPainter>
#include <KDebug>


AbstractProjectItem::AbstractProjectItem(const KUrl &url, AbstractProjectItem* parent) :
    QObject(parent)
    , m_parent(NULL)
    , m_name("loading")
    , m_isCurrent(false)
{
    if (url.isValid()) {
        m_name = url.fileName();
    }
    setParent(parent);
}

AbstractProjectItem::AbstractProjectItem(const QDomElement& description, AbstractProjectItem* parent) :
    QObject(parent)
    , m_parent(NULL)
    , m_isCurrent(false)
{
    m_name = description.attribute("name");
    m_description = description.attribute("description");

    setParent(parent);
}

AbstractProjectItem::~AbstractProjectItem()
{
    if (m_isCurrent) {
        if (bin()) {
            bin()->setCurrentItem(NULL);
        }
    }
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

void AbstractProjectItem::setParent(AbstractProjectItem* parent)
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

void AbstractProjectItem::finishInsert(AbstractProjectItem* /*parent*/)
{
    /*if (m_parent && !m_parent->contains(this)) {
        m_parent->addChild(this);
    }*/
    bin()->emitItemReady(this);
}

void AbstractProjectItem::addChild(AbstractProjectItem* child)
{
    if (child && !contains(child)) {
        bin()->emitAboutToAddItem(child);
        append(child);
	bin()->emitItemAdded(this);
    }
}

void AbstractProjectItem::removeChild(AbstractProjectItem* child)
{
    if (child && contains(child)) {
        bin()->emitAboutToRemoveItem(child);
        removeAll(child);
        bin()->emitItemAdded(child);
    }
}

int AbstractProjectItem::index() const
{
    if (m_parent) {
        return m_parent->indexOf(const_cast<AbstractProjectItem*>(this));
    }

    return 0;
}

BinModel* AbstractProjectItem::bin()
{
    if (m_parent) {
        return m_parent->bin();
    }
    return NULL;
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
        default:
            break;
    }
    return data;
}

int AbstractProjectItem::supportedDataCount() const
{
    return 2;
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

void AbstractProjectItem::setCurrent(bool current)
{
    if (m_isCurrent != current) 
    {
        m_isCurrent = current;
        if (current) {
            bin()->setCurrentItem(this);
        } else {
            bin()->setCurrentItem(NULL);
        }
    }
}

#include "abstractprojectitem.moc"
