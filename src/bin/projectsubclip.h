/*
Copyright (C) 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#ifndef PROJECTSUBCLIP_H
#define PROJECTSUBCLIP_H

#include "abstractprojectitem.h"
#include "definitions.h"
#include <memory>

class ProjectFolder;
class ProjectClip;
class QDomElement;

namespace Mlt {
}

/**
 * @class ProjectSubClip
 * @brief Represents a clip in the project (not timeline).
 *

 */

class ProjectSubClip : public AbstractProjectItem
{
    Q_OBJECT

public:
    /**
     * @brief Constructor; used when loading a project and the producer is already available.
     */
    static std::shared_ptr<ProjectSubClip> construct(const QString &id, const std::shared_ptr<ProjectClip> &parent,
                                                     const std::shared_ptr<ProjectItemModel> &model, int in, int out, const QString &timecode,
                                                     const QMap<QString, QString> zoneProperties);

protected:
    ProjectSubClip(const QString &id, const std::shared_ptr<ProjectClip> &parent, const std::shared_ptr<ProjectItemModel> &model, int in, int out,
                   const QString &timecode, const QMap<QString, QString> zoneProperties);

public:
    ~ProjectSubClip() override;

    std::shared_ptr<ProjectClip> clip(const QString &id) override;
    std::shared_ptr<ProjectFolder> folder(const QString &id) override;
    std::shared_ptr<ProjectSubClip> subClip(int in, int out);
    std::shared_ptr<ProjectClip> clipAt(int ix) override;
    /** @brief Recursively disable/enable bin effects. */
    void setBinEffectsEnabled(bool enabled) override;
    QDomElement toXml(QDomDocument &document, bool includeMeta = false, bool includeProfile = true) override;

    /** @brief Returns the clip's duration. */
    GenTime duration() const;

    /** @brief A string composed of: Clip id / in / out. */
    const QString cutClipId() const;

    /** @brief Sets thumbnail for this clip. */
    void setThumbnail(const QImage &);
    QPixmap thumbnail(int width, int height);
    /** @brief Sets a seeking thumbnail for this subclip. */
    void getThumbFromPercent(int percent);

    QPoint zone() const override;
    QString getToolTip() const override;
    bool rename(const QString &name, int column) override;
    /** @brief Returns true if item has both audio and video enabled. */
    bool hasAudioAndVideo() const override;

    /** @brief returns a pointer to the parent clip */
    std::shared_ptr<ProjectClip> getMasterClip() const;

    /** @brief Returns the clip type as defined in definitions.h */
    ClipType::ProducerType clipType() const override;

    /** @brief Set properties on this clip zone */
    void setProperties(const QMap<QString, QString> &properties);

    /** @brief Set rating on item */
    void setRating(uint rating) override;

private:
    std::shared_ptr<ProjectClip> m_masterClip;
    QString m_parentClipId;

private slots:
    void gotThumb(int pos, const QImage &img);
};

#endif
