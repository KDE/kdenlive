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
#include "utils/KoIconUtils.h"

#include <QDomElement>
#include <KLocalizedString>

ProjectFolder::ProjectFolder(const QString &id, const QString &name, ProjectFolder *parent) :
    AbstractProjectItem(AbstractProjectItem::FolderItem, id, parent)
    , m_bin(nullptr)
{
    m_name = name;
    m_clipStatus = StatusReady;
    m_thumbnail = KoIconUtils::themedIcon(QStringLiteral("folder"));
    setParent(parent);
}

ProjectFolder::ProjectFolder(Bin *bin) :
    AbstractProjectItem(AbstractProjectItem::FolderItem, QString::number(-1))
    , m_bin(bin)
{
    m_name = QStringLiteral("root");
    setParent(nullptr);
}

ProjectFolder::~ProjectFolder()
{
}

void ProjectFolder::setCurrent(bool current, bool notify)
{
    Q_UNUSED(notify)
    if (current) {
        bin()->openProducer(nullptr);
    }
}

ProjectClip *ProjectFolder::clip(const QString &id)
{
    for (int i = 0; i < count(); ++i) {
        ProjectClip *clip = at(i)->clip(id);
        if (clip) {
            return clip;
        }
    }
    return nullptr;
}

QList<ProjectClip *> ProjectFolder::childClips()
{
    QList<ProjectClip *> allChildren;
    for (int i = 0; i < count(); ++i) {
        AbstractProjectItem *child = at(i);
        if (child->itemType() == ClipItem) {
            allChildren << static_cast<ProjectClip *>(child);
        } else if (child->itemType() == FolderItem) {
            allChildren << static_cast<ProjectFolder *>(child)->childClips();
        }
    }
    return allChildren;
}

QString ProjectFolder::getToolTip() const
{
    return i18np("%1 clip", "%1 clips", size());
}

ProjectFolder *ProjectFolder::folder(const QString &id)
{
    if (m_id == id) {
        return this;
    }
    for (int i = 0; i < count(); ++i) {
        ProjectFolder *folderItem = at(i)->folder(id);
        if (folderItem) {
            return folderItem;
        }
    }
    return nullptr;
}

ProjectClip *ProjectFolder::clipAt(int index)
{
    if (isEmpty()) {
        return nullptr;
    }
    for (int i = 0; i < count(); ++i) {
        ProjectClip *clip = at(i)->clipAt(index);
        if (clip) {
            return clip;
        }
    }
    return nullptr;
}

void ProjectFolder::disableEffects(bool disable)
{
    for (int i = 0; i < count(); ++i) {
        AbstractProjectItem *item = at(i);
        item->disableEffects(disable);
    }
}

Bin *ProjectFolder::bin()
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

QDomElement ProjectFolder::toXml(QDomDocument &document, bool)
{
    QDomElement folder = document.createElement(QStringLiteral("folder"));
    folder.setAttribute(QStringLiteral("name"), name());
    for (int i = 0; i < count(); ++i) {
        folder.appendChild(at(i)->toXml(document));
    }
    return folder;
}

bool ProjectFolder::rename(const QString &name, int column)
{
    Q_UNUSED(column)
    if (m_name == name) {
        return false;
    }
    // Rename folder
    bin()->renameFolderCommand(m_id, name, m_name);
    return true;
}

