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

#ifndef PROJECTCLIP_H
#define PROJECTCLIP_H

#include "abstractprojectitem.h"
#include "definitions.h"
#include "mltcontroller/clipcontroller.h"
#include "timeline2/model/timelinemodel.hpp"

#include <QFuture>
#include <QMutex>
#include <QUrl>
#include <memory>

class AudioStreamInfo;
class ClipPropertiesController;
class MarkerListModel;
class ProjectFolder;
class ProjectSubClip;
class QDomElement;
class QUndoCommand;

namespace Mlt {
class Producer;
class Properties;
} // namespace Mlt

/**
 * @class ProjectClip
 * @brief Represents a clip in the project (not timeline).
 *
 */

class ProjectClip : public AbstractProjectItem, public ClipController
{
    Q_OBJECT

public:
    friend class Bin;
    friend bool TimelineModel::checkConsistency(); // for testing
    /**
     * @brief Constructor; used when loading a project and the producer is already available.
     */
    static std::shared_ptr<ProjectClip> construct(const QString &id, const QIcon &thumb, std::shared_ptr<ProjectItemModel> model,
                                                  std::shared_ptr<Mlt::Producer> producer);
    /**
     * @brief Constructor.
     * @param description element describing the clip; the "kdenlive:id" attribute and "resource" property are used
     */
    static std::shared_ptr<ProjectClip> construct(const QString &id, const QDomElement &description, const QIcon &thumb,
                                                  std::shared_ptr<ProjectItemModel> model);

protected:
    ProjectClip(const QString &id, const QIcon &thumb, std::shared_ptr<ProjectItemModel> model, std::shared_ptr<Mlt::Producer> producer);
    ProjectClip(const QString &id, const QDomElement &description, const QIcon &thumb, std::shared_ptr<ProjectItemModel> model);

public:
    virtual ~ProjectClip();

    void reloadProducer(bool refreshOnly = false);

    /** @brief Returns a unique hash identifier used to store clip thumbnails. */
    // virtual void hash() = 0;

    /** @brief Returns this if @param id matches the clip's id or nullptr otherwise. */
    std::shared_ptr<ProjectClip> clip(const QString &id) override;

    std::shared_ptr<ProjectFolder> folder(const QString &id) override;

    std::shared_ptr<ProjectSubClip> getSubClip(int in, int out);

    /** @brief Returns this if @param ix matches the clip's index or nullptr otherwise. */
    std::shared_ptr<ProjectClip> clipAt(int ix) override;

    /** @brief Returns the clip type as defined in definitions.h */
    ClipType clipType() const;

    bool selfSoftDelete(Fun &undo, Fun &redo) override;

    /** @brief Check if clip has a parent folder with id id */
    bool hasParent(const QString &id) const;
    ClipPropertiesController *buildProperties(QWidget *parent);
    QPoint zone() const override;

    /** @brief Returns true if we want to add an affine transition in timeline when dropping this clip. */
    bool isTransparent() const;

    /** @brief Returns whether this clip has a url (=describes a file) or not. */
    bool hasUrl() const;

    /** @brief Returns the clip's url. */
    const QString url() const;

    /** @brief Returns the clip's duration. */
    GenTime duration() const;
    int frameDuration() const;

    /** @brief Returns the original clip's fps. */
    double getOriginalFps() const;

    bool rename(const QString &name, int column) override;

    QDomElement toXml(QDomDocument &document, bool includeMeta = false) override;

    // QVariant getData(DataType type) const override;

    /** @brief Sets thumbnail for this clip. */
    void setThumbnail(const QImage &);
    QPixmap thumbnail(int width, int height);

    /** @brief Sets the MLT producer associated with this clip
     *  @param producer The producer
     *  @param replaceProducer If true, we replace existing producer with this one
     *  @returns true if producer was changed
     * . */
    bool setProducer(std::shared_ptr<Mlt::Producer> producer, bool replaceProducer);

    /** @brief Returns true if this clip already has a producer. */
    bool isReady() const;

    /** @brief Returns this clip's producer. */
    std::shared_ptr< Mlt::Producer > thumbProducer();

    /** @brief Recursively disable/enable bin effects. */
    void setBinEffectsEnabled(bool enabled) override;

    /** @brief Set properties on this clip. TODO: should we store all in MLT or use extra m_properties ?. */
    void setProperties(const QMap<QString, QString> &properties, bool refreshPanel = false);

    /** @brief Get an XML property from MLT produced xml. */
    static QString getXmlProperty(const QDomElement &producer, const QString &propertyName, const QString &defaultValue = QString());

    QString getToolTip() const override;

    /** @brief The clip hash created from the clip's resource. */
    const QString hash();

    /** @brief Returns true if we are using a proxy for this clip. */
    bool hasProxy() const;

    /** Cache for every audio Frame with 10 Bytes */
    /** format is frame -> channel ->bytes */
    QVariantList audioFrameCache;
    bool audioThumbCreated() const;

    void setWaitingStatus(const QString &id);
    /** @brief Returns true if the clip matched a condition, for example vcodec=mpeg1video. */
    bool matches(const QString &condition);

    /** @brief Returns the number of audio channels. */
    int audioChannels() const;
    /** @brief get data analysis value. */
    QStringList updatedAnalysisData(const QString &name, const QString &data, int offset);
    QMap<QString, QString> analysisData(bool withPrefix = false);
    /** @brief Returns the list of this clip's subclip's ids. */
    QStringList subClipIds() const;
    /** @brief Delete cached audio thumb - needs to be recreated */
    void discardAudioThumb();
    /** @brief Get path for this clip's audio thumbnail */
    const QString getAudioThumbPath();
    /** @brief Returns true if this producer has audio and can be splitted on timeline*/
    bool isSplittable() const;

    /** @brief Returns true if a clip corresponding to this bin is inserted in a timeline.
        Note that this function does not account for children, use TreeItem::accumulate if you want to get that information as well.
    */
    bool isIncludedInTimeline() override;
    /** @brief Returns a list of all timeline clip ids for this bin clip */
    QList<int> timelineInstances() const;
    std::shared_ptr<Mlt::Producer> timelineProducer(PlaylistState::ClipState state = PlaylistState::Original, int track = 1);
    std::shared_ptr<Mlt::Producer> cloneProducer(Mlt::Profile *destProfile = nullptr);
    void updateTimelineClips(QVector<int> roles);

protected:
    friend class ClipModel;
    /** @brief This is a call-back called by a ClipModel when it is created
        @param timeline ptr to the pointer in which this ClipModel is inserted
        @param clipId id of the inserted clip
     */
    void registerTimelineClip(std::weak_ptr<TimelineModel> timeline, int clipId);

    /* @brief update the producer to reflect new parent folder */
    void updateParent(std::shared_ptr<TreeItem> parent) override;

    /** @brief This is a call-back called by a ClipModel when it is deleted
        @param clipId id of the deleted clip
    */
    void deregisterTimelineClip(int clipId);

    void emitProducerChanged(const QString &id, const std::shared_ptr<Mlt::Producer> &producer) override { emit producerChanged(id, producer); };
    /** @brief Replace instance of this clip in timeline */
    void updateChildProducers();
    void replaceInTimeline();
    void connectEffectStack();

public slots:
    /* @brief Store the audio thumbnails once computed. Note that the parameter is a value and not a reference, fill free to use it as a sink (use std::move to
     * avoid copy). */
    void updateAudioThumbnail(QVariantList audioLevels);
    /** @brief Extract image thumbnails for timeline. */
    void slotExtractImage(const QList<int> &frames);

private:
    /** @brief Generate and store file hash if not available. */
    const QString getFileHash();
    /** @brief Store clip url temporarily while the clip controller has not been created. */
    QString m_temporaryUrl;
    std::shared_ptr<Mlt::Producer> m_thumbsProducer;
    QMutex m_producerMutex;
    QMutex m_thumbMutex;
    QFuture<void> m_thumbThread;
    QList<int> m_requestedThumbs;
    const QString geometryWithOffset(const QString &data, int offset);
    void doExtractImage();

    std::map<int, std::weak_ptr<TimelineModel>> m_registeredClips;
    std::map<int, std::shared_ptr<Mlt::Producer>> m_timelineProducers;

signals:
    void producerChanged(const QString &, const std::shared_ptr<Mlt::Producer> &);
    void refreshPropertiesPanel();
    void refreshAnalysisPanel();
    void refreshClipDisplay();
    void thumbReady(int, const QImage &);
    /** @brief Clip is ready, load properties. */
    void loadPropertiesPanel();
};

#endif
