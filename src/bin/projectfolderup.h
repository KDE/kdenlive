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

#ifndef PROJECTFOLDERUP_H
#define PROJECTFOLDERUP_H

#include "abstractprojectitem.h"

/**
 * @class ProjectFolderUpUp
 * @brief A simple "folder up" item allowing to navigate up when the bin is in icon view.
 */

class ProjectClip;
class Bin;

class ProjectFolderUp : public AbstractProjectItem
{
    Q_OBJECT

public:
    /**
     * @brief Creates the supplied folder and loads its children.
     * @param description element describing the folder and its children
     * @param parent parent folder
     */
    explicit ProjectFolderUp(AbstractProjectItem *parent);

    ~ProjectFolderUp();

    /**
     * @brief Returns the clip if it is a child (also indirect).
     * @param id id of the child which should be returned
     */
    ProjectClip *clip(const QString &id) Q_DECL_OVERRIDE;

    /**
     * @brief Returns itself or a child folder that matches the requested id.
     * @param id id of the child which should be returned
     */
    ProjectFolder *folder(const QString &id) Q_DECL_OVERRIDE;

    /** @brief Calls AbstractProjectItem::setCurrent and blank the bin monitor. */
    void setCurrent(bool current, bool notify = true) Q_DECL_OVERRIDE;

    /**
     * @brief Returns the clip if it is a child (also indirect).
     * @param index index of the child which should be returned
     */
    ProjectClip *clipAt(int index) Q_DECL_OVERRIDE;

    /** @brief Recursively disable/enable bin effects. */
    void disableEffects(bool disable) Q_DECL_OVERRIDE;

    /** @brief Returns a pointer to the bin model this folder belongs to. */
    Bin *bin() Q_DECL_OVERRIDE;

    /** @brief Returns an xml description of the folder. */
    QDomElement toXml(QDomDocument &document, bool includeMeta = false) Q_DECL_OVERRIDE;
    QString getToolTip() const Q_DECL_OVERRIDE;
    bool rename(const QString &name, int column) Q_DECL_OVERRIDE;

private:
    Bin *m_bin;
};

#endif
