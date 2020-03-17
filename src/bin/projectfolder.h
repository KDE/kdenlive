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

#ifndef PROJECTFOLDER_H
#define PROJECTFOLDER_H

#include "abstractprojectitem.h"

/**
 * @class ProjectFolder
 * @brief A folder in the bin.
 */

class ProjectClip;
class Bin;

class ProjectFolder : public AbstractProjectItem
{
    Q_OBJECT

public:
    /**
     * @brief Creates the supplied folder and loads its children.
     * @param description element describing the folder and its children
     */
    static std::shared_ptr<ProjectFolder> construct(const QString &id, const QString &name, std::shared_ptr<ProjectItemModel> model);

    /** @brief Creates an empty root folder. */
    static std::shared_ptr<ProjectFolder> construct(std::shared_ptr<ProjectItemModel> model);

protected:
    ProjectFolder(const QString &id, const QString &name, const std::shared_ptr<ProjectItemModel> &model);

    explicit ProjectFolder(const std::shared_ptr<ProjectItemModel> &model);

public:
    ~ProjectFolder() override;

    /**
     * @brief Returns the clip if it is a child (also indirect).
     * @param id id of the child which should be returned
     */
    std::shared_ptr<ProjectClip> clip(const QString &id) override;

    /**
     * @brief Returns itself or a child folder that matches the requested id.
     * @param id id of the child which should be returned
     */
    std::shared_ptr<ProjectFolder> folder(const QString &id) override;

    /** @brief Recursively disable/enable bin effects. */
    void setBinEffectsEnabled(bool enabled) override;

    /**
     * @brief Returns the clip if it is a child (also indirect).
     * @param index index of the child which should be returned
     */
    std::shared_ptr<ProjectClip> clipAt(int index) override;

    /** @brief Returns an xml description of the folder. */
    QDomElement toXml(QDomDocument &document, bool includeMeta = false, bool includeProfile = true) override;
    QString getToolTip() const override;
    bool rename(const QString &name, int column) override;
    /** @brief Returns a list of all children and sub-children clips. */
    QList<std::shared_ptr<ProjectClip>> childClips();
    /** @brief Returns true if folder contains a clip. */
    bool hasChildClips() const;
    ClipType::ProducerType clipType() const override;
    /** @brief Returns true if item has both audio and video enabled. */
    bool hasAudioAndVideo() const override;
    /** @brief Returns a clip id if folder contains clip with matching @hash, empty if not found. */
    QString childByHash(const QString &hash);
};

#endif
