/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "projectfolder.h"
#include "core.h"
#include "clippluginmanager.h"
#include "abstractprojectclip.h"
#include "project.h"
#include <QDomElement>


ProjectFolder::ProjectFolder(const QDomElement& description, AbstractProjectItem* parent) :
    AbstractProjectItem(description, parent),
    m_project(NULL)
{
    loadChildren(description);
}

ProjectFolder::ProjectFolder(const QDomElement& description, Project* project) :
    AbstractProjectItem(description),
    m_project(project)
{
    loadChildren(description);
}

ProjectFolder::ProjectFolder(AbstractProjectItem* parent) :
    AbstractProjectItem(parent)
{
}

ProjectFolder::ProjectFolder(Project* project) :
    AbstractProjectItem(),
    m_project(project)
{
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

Project* ProjectFolder::project()
{
    if (m_project) {
        return m_project;
    } else {
        return AbstractProjectItem::project();
    }
}

void ProjectFolder::loadChildren(const QDomElement& description)
{
    QDomNodeList childen = description.childNodes();
    for (int i = 0; i < childen.count(); ++i) {
        QDomElement childElement = childen.at(i).toElement();
        if (childElement.tagName() == "folder") {
            new ProjectFolder(childElement, this);
        } else {
            pCore->clipPluginManager()->loadClip(childElement, this);
        }
    }
}

#include "projectfolder.moc"
