/*
SPDX-FileCopyrightText: 2012 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "projectfolder.h"
#include "bin.h"
#include "core.h"
#include "projectclip.h"
#include "projectitemmodel.h"

#include <KLocalizedString>
#include <QDomElement>
#include <utility>
ProjectFolder::ProjectFolder(const QString &id, const QString &name, const std::shared_ptr<ProjectItemModel> &model)
    : AbstractProjectItem(AbstractProjectItem::FolderItem, id, model)
{
    m_name = name;
    m_clipStatus = FileStatus::StatusReady;
}

std::shared_ptr<ProjectFolder> ProjectFolder::construct(const QString &id, const QString &name, std::shared_ptr<ProjectItemModel> model)
{
    std::shared_ptr<ProjectFolder> self(new ProjectFolder(id, name, std::move(model)));

    baseFinishConstruct(self);
    return self;
}

ProjectFolder::ProjectFolder(const std::shared_ptr<ProjectItemModel> &model)
    : AbstractProjectItem(AbstractProjectItem::FolderItem, QString::number(-1), model, true)
{
    m_name = QStringLiteral("root");
}

std::shared_ptr<ProjectFolder> ProjectFolder::construct(std::shared_ptr<ProjectItemModel> model)
{
    std::shared_ptr<ProjectFolder> self(new ProjectFolder(std::move(model)));
    baseFinishConstruct(self);
    return self;
}

ProjectFolder::~ProjectFolder() = default;

std::shared_ptr<ProjectClip> ProjectFolder::clip(const QString &id)
{
    for (int i = 0; i < childCount(); ++i) {
        std::shared_ptr<ProjectClip> clip = std::static_pointer_cast<ProjectClip>(child(i))->clip(id);
        if (clip) {
            return clip;
        }
    }
    return std::shared_ptr<ProjectClip>();
}

QList<std::shared_ptr<ProjectClip>> ProjectFolder::childClips()
{
    QList<std::shared_ptr<ProjectClip>> allChildren;
    for (int i = 0; i < childCount(); ++i) {
        std::shared_ptr<AbstractProjectItem> childItem = std::static_pointer_cast<AbstractProjectItem>(child(i));
        if (childItem->itemType() == ClipItem) {
            allChildren << std::static_pointer_cast<ProjectClip>(childItem);
        } else if (childItem->itemType() == FolderItem) {
            allChildren << std::static_pointer_cast<ProjectFolder>(childItem)->childClips();
        }
    }
    return allChildren;
}

QString ProjectFolder::childByHash(const QString &hash)
{
    QList<std::shared_ptr<ProjectClip>> allChildren;
    for (int i = 0; i < childCount(); ++i) {
        std::shared_ptr<AbstractProjectItem> childItem = std::static_pointer_cast<AbstractProjectItem>(child(i));
        if (childItem->itemType() == ClipItem) {
            allChildren << std::static_pointer_cast<ProjectClip>(childItem);
        } else if (childItem->itemType() == FolderItem) {
            allChildren << std::static_pointer_cast<ProjectFolder>(childItem)->childClips();
        }
    }
    for (auto &clip : allChildren) {
        if (clip->statusReady() && clip->hash() == hash) {
            return clip->clipId();
        }
    }
    return QString();
}

bool ProjectFolder::hasChildClips() const
{
    for (int i = 0; i < childCount(); ++i) {
        std::shared_ptr<AbstractProjectItem> childItem = std::static_pointer_cast<AbstractProjectItem>(child(i));
        if (childItem->itemType() == ClipItem) {
            return true;
        }
        if (childItem->itemType() == FolderItem) {
            bool hasChildren = std::static_pointer_cast<ProjectFolder>(childItem)->hasChildClips();
            if (hasChildren) {
                return true;
            }
        }
    }
    return false;
}

QString ProjectFolder::getToolTip() const
{
    return i18np("%1 clip", "%1 clips", childCount());
}

std::shared_ptr<ProjectFolder> ProjectFolder::folder(const QString &id)
{
    if (m_binId == id) {
        return std::static_pointer_cast<ProjectFolder>(shared_from_this());
    }
    for (int i = 0; i < childCount(); ++i) {
        std::shared_ptr<ProjectFolder> folderItem = std::static_pointer_cast<ProjectFolder>(child(i))->folder(id);
        if (folderItem) {
            return folderItem;
        }
    }
    return std::shared_ptr<ProjectFolder>();
}

std::shared_ptr<ProjectClip> ProjectFolder::clipAt(int index)
{
    if (childCount() == 0) {
        return std::shared_ptr<ProjectClip>();
    }
    for (int i = 0; i < childCount(); ++i) {
        std::shared_ptr<ProjectClip> clip = std::static_pointer_cast<AbstractProjectItem>(child(i))->clipAt(index);
        if (clip) {
            return clip;
        }
    }
    return std::shared_ptr<ProjectClip>();
}

void ProjectFolder::setBinEffectsEnabled(bool enabled)
{
    for (int i = 0; i < childCount(); ++i) {
        std::shared_ptr<AbstractProjectItem> item = std::static_pointer_cast<AbstractProjectItem>(child(i));
        item->setBinEffectsEnabled(enabled);
    }
}

QDomElement ProjectFolder::toXml(QDomDocument &document, bool, bool)
{
    QDomElement folder = document.createElement(QStringLiteral("folder"));
    folder.setAttribute(QStringLiteral("name"), name());
    for (int i = 0; i < childCount(); ++i) {
        folder.appendChild(std::static_pointer_cast<AbstractProjectItem>(child(i))->toXml(document));
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
    if (auto ptr = m_model.lock()) {
        auto self = std::static_pointer_cast<ProjectFolder>(shared_from_this());
        return std::static_pointer_cast<ProjectItemModel>(ptr)->requestRenameFolder(self, name);
    }
    qDebug() << "ERROR: Impossible to rename folder because model is not available";
    Q_ASSERT(false);
    return false;
}

ClipType::ProducerType ProjectFolder::clipType() const
{
    return ClipType::Unknown;
}

bool ProjectFolder::hasAudioAndVideo() const
{
    return false;
}
