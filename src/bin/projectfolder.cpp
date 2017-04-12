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
#include "projectitemmodel.h"
#include "bin.h"
#include "utils/KoIconUtils.h"

#include <QDomElement>
#include <KLocalizedString>

ProjectFolder::ProjectFolder(const QString &id, const QString &name, ProjectItemModel* model, ProjectFolder *parent) :
    AbstractProjectItem(AbstractProjectItem::FolderItem, id, model, parent)
{
    m_name = name;
    m_clipStatus = StatusReady;
    m_thumbnail = KoIconUtils::themedIcon(QStringLiteral("folder"));
    changeParent(parent);
}

ProjectFolder::ProjectFolder(ProjectItemModel* model) :
    AbstractProjectItem(AbstractProjectItem::FolderItem, QString::number(-1),  model)
{
    m_name = QStringLiteral("root");
    changeParent(nullptr);
}

ProjectFolder::~ProjectFolder()
{
}

void ProjectFolder::setCurrent(bool current, bool notify)
{
    Q_UNUSED(notify);
    Q_UNUSED(current)
}

ProjectClip *ProjectFolder::clip(const QString &id)
{
    for (int i = 0; i < childCount(); ++i) {
        ProjectClip *clip = static_cast<AbstractProjectItem*>(child(i))->clip(id);
        if (clip) {
            return clip;
        }
    }
    return nullptr;
}

QList<ProjectClip *> ProjectFolder::childClips()
{
    QList<ProjectClip *> allChildren;
    for (int i = 0; i < childCount(); ++i) {
        AbstractProjectItem *childItem = static_cast<AbstractProjectItem*>(child(i));
        if (childItem->itemType() == ClipItem) {
            allChildren << static_cast<ProjectClip *>(childItem);
        } else if (childItem->itemType() == FolderItem) {
            allChildren << static_cast<ProjectFolder *>(childItem)->childClips();
        }
    }
    return allChildren;
}

QString ProjectFolder::getToolTip() const
{
    return i18np("%1 clip", "%1 clips", childCount());
}

ProjectFolder *ProjectFolder::folder(const QString &id)
{
    if (m_id == id) {
        return this;
    }
    for (int i = 0; i < childCount(); ++i) {
        ProjectFolder *folderItem = static_cast<AbstractProjectItem*>(child(i))->folder(id);
        if (folderItem) {
            return folderItem;
        }
    }
    return nullptr;
}

ProjectClip *ProjectFolder::clipAt(int index)
{
    if (childCount() == 0) {
        return nullptr;
    }
    for (int i = 0; i < childCount(); ++i) {
        ProjectClip *clip = static_cast<AbstractProjectItem*>(child(i))->clipAt(index);
        if (clip) {
            return clip;
        }
    }
    return nullptr;
}

void ProjectFolder::disableEffects(bool disable)
{
    for (int i = 0; i < childCount(); ++i) {
        AbstractProjectItem *item = static_cast<AbstractProjectItem*>(child(i));
        item->disableEffects(disable);
    }
}


QDomElement ProjectFolder::toXml(QDomDocument &document, bool)
{
    QDomElement folder = document.createElement(QStringLiteral("folder"));
    folder.setAttribute(QStringLiteral("name"), name());
    for (int i = 0; i < childCount(); ++i) {
        folder.appendChild(static_cast<AbstractProjectItem*>(child(i))->toXml(document));
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
    static_cast<ProjectItemModel*>(m_model)->bin()->renameFolderCommand(m_id, name, m_name);
    return true;
}

