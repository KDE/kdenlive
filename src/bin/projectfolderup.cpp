/*
Copyright (C) 2015  Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#include "projectfolderup.h"
#include "projectclip.h"
#include "utils/KoIconUtils.h"

#include <KLocalizedString>
#include <QDomElement>

ProjectFolderUp::ProjectFolderUp(std::shared_ptr<ProjectItemModel> model)
    : AbstractProjectItem(AbstractProjectItem::FolderUpItem, QString(), model)
{
    m_thumbnail = KoIconUtils::themedIcon(QStringLiteral("go-previous"));
    m_name = i18n("Back");
}

std::shared_ptr<ProjectFolderUp> ProjectFolderUp::construct(std::shared_ptr<ProjectItemModel> model)
{
    std::shared_ptr<ProjectFolderUp> self(new ProjectFolderUp(model));

    baseFinishConstruct(self);
    return self;
}

ProjectFolderUp::~ProjectFolderUp()
{
}

std::shared_ptr<ProjectClip> ProjectFolderUp::clip(const QString &id)
{
    Q_UNUSED(id)
    return std::shared_ptr<ProjectClip>();
}

QString ProjectFolderUp::getToolTip() const
{
    return i18n("Go up");
}

std::shared_ptr<ProjectFolder> ProjectFolderUp::folder(const QString &id)
{
    Q_UNUSED(id);
    return std::shared_ptr<ProjectFolder>();
}

std::shared_ptr<ProjectClip> ProjectFolderUp::clipAt(int index)
{
    Q_UNUSED(index);
    return std::shared_ptr<ProjectClip>();
}

void ProjectFolderUp::setBinEffectsEnabled(bool)
{
}

QDomElement ProjectFolderUp::toXml(QDomDocument &document, bool)
{
    return document.documentElement();
}

bool ProjectFolderUp::rename(const QString &, int)
{
    return false;
}
