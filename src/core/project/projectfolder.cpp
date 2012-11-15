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
#include "binmodel.h"
#include <QDomElement>


ProjectFolder::ProjectFolder(const QDomElement& description, ProjectFolder* parent) :
    AbstractProjectItem(description, parent),
    m_bin(NULL)
{
    loadChildren(description);
}

ProjectFolder::ProjectFolder(const QDomElement& description, BinModel* bin) :
    AbstractProjectItem(description),
    m_bin(bin)
{
    loadChildren(description);
}

ProjectFolder::ProjectFolder(ProjectFolder* parent) :
    AbstractProjectItem(parent)
{
}

ProjectFolder::ProjectFolder(BinModel* bin) :
    AbstractProjectItem(),
    m_bin(bin)
{
}

ProjectFolder::~ProjectFolder()
{
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

BinModel* ProjectFolder::bin()
{
    if (m_bin) {
        return m_bin;
    } else {
        return AbstractProjectItem::bin();
    }
}

QDomElement ProjectFolder::toXml(QDomDocument& document) const
{
    QDomElement folder = document.createElement("folder");
    folder.setAttribute("name", name());

    for (int i = 0; i < count(); ++i) {
        folder.appendChild(at(i)->toXml(document));
    }

    return folder;
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
