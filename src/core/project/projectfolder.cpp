/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "projectfolder.h"
#include "clippluginmanager.h"
#include "abstractprojectclip.h"
#include <QDomElement>

#include <KDebug>


ProjectFolder::ProjectFolder(const QDomElement& description, ClipPluginManager *clipPluginManager, AbstractProjectItem* parent) :
    AbstractProjectItem(description, parent)
{
    if (description.tagName() == "project_items") {
        kDebug() << "root folder created";
    } else kDebug() << "folder created" << m_name;

    QDomNodeList childen = description.childNodes();
    for (int i = 0; i < childen.count(); ++i) {
        QDomElement childElement = childen.at(i).toElement();
        AbstractProjectItem *child;
        if (childElement.tagName() == "folder") {
            child = new ProjectFolder(childElement, clipPluginManager, this);
        } else {
            child = clipPluginManager->loadClip(childElement, this);
        }
        if (child) {
            append(child);
        }
    }
}

ProjectFolder::~ProjectFolder()
{
    qDeleteAll(begin(), end());
}


AbstractProjectClip* ProjectFolder::clip(int id)
{
    AbstractProjectClip *clip;
    for (int i = 0; i < count(); ++i) {
        clip = at(i)->clip(id);
        if (clip) {
            return clip;
        }
    }
    return NULL;
}

#include "projectfolder.moc"
