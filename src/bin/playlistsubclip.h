/*
SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractprojectitem.h"
#include "definitions.h"
#include <memory>

class ProjectFolder;
class ProjectClip;
class QDomElement;

namespace Mlt {
}

/** @class PlaylistSubClip
    @brief Represents a clip in the project (not timeline).
  */
class PlaylistSubClip : public AbstractProjectItem
{
    Q_OBJECT

public:
    /**
     * @brief Constructor; used when loading a project and the producer is already available.
     */
    static std::shared_ptr<PlaylistSubClip> construct(const QString &id, const std::shared_ptr<ProjectClip> &parent,
                                                      const std::shared_ptr<ProjectItemModel> &model, const QString &timecode,
                                                      const QMap<QString, QString> &zoneProperties);

protected:
    PlaylistSubClip(const QString &id, const std::shared_ptr<ProjectClip> &parent, const std::shared_ptr<ProjectItemModel> &model, const QString &timecode,
                    const QMap<QString, QString> &zoneProperties);

public:
    ~PlaylistSubClip() override;

    std::shared_ptr<ProjectClip> clip(const QString &id) override;
    std::shared_ptr<ProjectFolder> folder(const QString &id) override;
    std::shared_ptr<PlaylistSubClip> subClip(int in, int out);
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
    const QString &clipId(bool withSequence = false) const override;

    /** @brief returns a pointer to the parent clip */
    std::shared_ptr<ProjectClip> getMasterClip() const;

    /** @brief Returns the clip type as defined in definitions.h */
    ClipType::ProducerType clipType() const override;
    const QUuid sequenceUuid() const;

    /** @brief Set properties on this clip zone */
    void setProperties(const QMap<QString, QString> &properties);

    /** @brief Set rating on item */
    void setRating(uint rating) override;

private:
    std::shared_ptr<ProjectClip> m_masterClip;
    QString m_parentClipId;
    QString m_clipIdWithSequence;
    QUuid m_sequenceUuid;
    QString m_tmpPlaylistPath;

private Q_SLOTS:
    void gotThumb(int pos, const QImage &img);
};
