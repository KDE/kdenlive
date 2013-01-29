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
#include "monitor/monitormodel.h"
#include <KLocalizedString>
#include <QDomElement>


BinModel::BinModel(Project* parent) :
    QObject(parent),
    m_project(parent),
    m_currentItem(NULL)
{
    m_rootFolder = new ProjectFolder(this);
    m_monitor = new MonitorModel(m_project->profile(), i18n("Bin"), this);
}

BinModel::BinModel(const QDomElement&element, Project* parent) :
    QObject(parent),
    m_project(parent),
    m_currentItem(NULL)
{
    m_rootFolder = new ProjectFolder(element.firstChildElement("folder"), this);
    m_monitor = new MonitorModel(m_project->profile(), i18n("Bin"), this);
}

Project* BinModel::project()
{
    return m_project;
}

MonitorModel* BinModel::monitor()
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

AbstractProjectItem* BinModel::currentItem()
{
    return m_currentItem;
}

void BinModel::setCurrentItem(AbstractProjectItem* item)
{
    if (m_currentItem != item) {
        AbstractProjectItem *oldItem = m_currentItem;
        m_currentItem = item;
        if (oldItem) {
            oldItem->setCurrent(false);
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

void BinModel::emitItemAdded(AbstractProjectItem* item)
{
    emit itemAdded(item);
}

void BinModel::emitAboutToRemoveItem(AbstractProjectItem* item)
{
    emit aboutToRemoveItem(item);
}

void BinModel::emitItemRemoved(AbstractProjectItem* item)
{
    emit itemRemoved(item);
}
