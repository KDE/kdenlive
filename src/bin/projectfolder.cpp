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

#include "projectfolder.h"
#include "projectclip.h"
#include "bin.h"

#include <QDomElement>
#include <KLocalizedString>

ProjectFolder::ProjectFolder(const QString &id, const QString &name, ProjectFolder* parent) :
    AbstractProjectItem(AbstractProjectItem::FolderItem, id, parent)
    , m_bin(NULL)
{
    //loadChildren(description);
    m_name = name;
    m_thumbnail = QIcon::fromTheme("folder").pixmap(40, 40);
}

ProjectFolder::ProjectFolder(Bin *bin) :
    AbstractProjectItem(AbstractProjectItem::FolderItem, QString::number(-1))
    , m_bin(bin)
{
}

ProjectFolder::~ProjectFolder()
{
}


ProjectClip* ProjectFolder::clip(const QString &id)
{
    ProjectClip *clip;
    for (int i = 0; i < count(); ++i) {
        clip = at(i)->clip(id);
        if (clip) {
            return clip;
        }
    }
    return NULL;
}

ProjectFolder* ProjectFolder::folder(const QString &id)
{
    if (m_id == id) return this;
    ProjectFolder *folderItem;
    for (int i = 0; i < count(); ++i) {
        folderItem = at(i)->folder(id);
        if (folderItem) {
            return folderItem;
        }
    }
    return NULL;
}

ProjectClip* ProjectFolder::clipAt(int index)
{
    ProjectClip *clip;
    if (isEmpty()) return NULL;
    for (int i = 0; i < count(); ++i) {
        clip = at(i)->clipAt(index);
        if (clip) {
            return clip;
        }
    }
    return NULL;
}

Bin* ProjectFolder::bin()
{
    if (m_bin) {
        return m_bin;
    } else {
        if (parent()) {
            return parent()->bin();
        }
        return AbstractProjectItem::bin();
    }
}

QDomElement ProjectFolder::toXml(QDomDocument& document)
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
    /*QDomNodeList childen = description.childNodes();
    for (int i = 0; i < childen.count(); ++i) {
        QDomElement childElement = childen.at(i).toElement();
        if (childElement.tagName() == "folder") {
            new ProjectFolder(childElement, this);
        } else {
            childElement.setTagName("producer");
        }
    }*/
}

