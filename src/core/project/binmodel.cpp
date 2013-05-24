/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "binmodel.h"
#include "project.h"
#include "projectfolder.h"
#include "abstractprojectclip.h"
#include "monitor/glwidget.h"
#include <KLocalizedString>
#include <QDomElement>
#include <KDebug>


BinModel::BinModel(Project* parent) :
    QObject(parent),
    m_project(parent),
    m_currentItem(NULL),
    m_monitor(NULL)
{
    m_rootFolder = new ProjectFolder(this);
    //m_monitor = new MonitorModel(m_project->profile(), i18n("Bin"), this);
}

BinModel::BinModel(const QDomElement&element, Project* parent) :
    QObject(parent),
    m_project(parent),
    m_currentItem(NULL),
    m_monitor(NULL)
{
    m_rootFolder = new ProjectFolder(element.firstChildElement("folder"), this);
    //m_monitor = new MonitorModel(m_project->profile(), i18n("Bin"), this);
}

Project* BinModel::project()
{
    return m_project;
}

void BinModel::setMonitor(MonitorView* m)
{
    m_monitor = m;
}

MonitorView* BinModel::monitor()
{
    return m_monitor;
}

ProjectFolder* BinModel::rootFolder()
{
    return m_rootFolder;
}

AbstractProjectClip* BinModel::clip(const QString &id)
{
    return m_rootFolder->clip(id);
}

void BinModel::slotGotImage(const QString &id, int pos, QImage img)
{
    AbstractProjectClip* c = clip(id);
    if (c) {
	c->setThumbnail(img);
    }
}

ProducerWrapper *BinModel::clipProducer(const QString &id)
{
    AbstractProjectClip* c = clip(id);
    if (c) {
	return c->baseProducer();
    }
    return NULL;
}

AbstractProjectItem* BinModel::currentItem()
{
    return m_currentItem;
}

void BinModel::setCurrentItem(AbstractProjectItem* item)
{
    if (item) kDebug()<<" + + + + + + + ++ BIN MODEL SET CURR + + + : "<<item->name();
    if (m_currentItem != item) {
        AbstractProjectItem *oldItem = m_currentItem;
        m_currentItem = item;
        if (oldItem) {
            oldItem->setCurrent(false);
        }
        if (m_currentItem) {
	    m_currentItem->setCurrent(true);
	    /*AbstractProjectClip *pclip = static_cast <AbstractProjectClip*>(m_currentItem);
	    pclip->setCurrent(true);*/
	}

        emit currentItemChanged(item);
    }
}

QDomElement BinModel::toXml(QDomDocument& document) const
{
    QDomElement bin = document.createElement("bin");
    bin.appendChild(m_rootFolder->toXml(document));
    return bin;
}

void BinModel::emitAboutToAddItem(AbstractProjectItem* item)
{
    emit aboutToAddItem(item);
}

void BinModel::emitItemReady(AbstractProjectItem* item)
{
    m_currentItem->setCurrent(true);
}

void BinModel::emitItemAdded(AbstractProjectItem* item)
{
    emit itemAdded(item);
    /*AbstractProjectItem* c = m_rootFolder->clipAt(item->index());
    kDebug()<<"// ++ ++ ++ ++ ADDED item: "<<item->index();
    if (c) {
	kDebug()<<" // / /ITEM NAME: "<<c->name();
	setCurrentItem(c);
    }*/
    kDebug()<<"------------------------------------";
    
    //if (item) setCurrentItem(item);
}

void BinModel::emitAboutToRemoveItem(AbstractProjectItem* item)
{
    emit aboutToRemoveItem(item);
}

void BinModel::emitItemRemoved(AbstractProjectItem* item)
{
    emit itemRemoved(item);
}

void BinModel::emitItemUpdated(AbstractProjectItem* item)
{
    emit itemUpdated(item);
}

