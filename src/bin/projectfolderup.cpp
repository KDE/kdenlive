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
#include "bin.h"
#include "utils/KoIconUtils.h"

#include <QDomElement>
#include <KLocalizedString>

ProjectFolderUp::ProjectFolderUp(AbstractProjectItem *parent) :
    AbstractProjectItem(AbstractProjectItem::FolderUpItem, QString(), parent)
    , m_bin(nullptr)
{
    m_thumbnail = KoIconUtils::themedIcon(QStringLiteral("go-previous"));
    m_name = i18n("Back");
    setParent(parent);
}

ProjectFolderUp::~ProjectFolderUp()
{
}

void ProjectFolderUp::setCurrent(bool current, bool notify)
{
    Q_UNUSED(current)
    Q_UNUSED(notify)
}

ProjectClip *ProjectFolderUp::clip(const QString &id)
{
    Q_UNUSED(id)
    return nullptr;
}

QString ProjectFolderUp::getToolTip() const
{
    return i18n("Go up");
}

ProjectFolder *ProjectFolderUp::folder(const QString &id)
{
    Q_UNUSED(id)
    return nullptr;
}

ProjectClip *ProjectFolderUp::clipAt(int index)
{
    Q_UNUSED(index)
    return nullptr;
}

void ProjectFolderUp::disableEffects(bool)
{
}

Bin *ProjectFolderUp::bin()
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

QDomElement ProjectFolderUp::toXml(QDomDocument &document, bool)
{
    return document.documentElement();
}

bool ProjectFolderUp::rename(const QString &, int)
{
    return false;
}
