/*
SPDX-FileCopyrightText: 2012 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractprojectitem.h"
#include "definitions.h"
#include "mltcontroller/clipcontroller.h"
#include "timeline2/model/timelinemodel.hpp"

#include <QFuture>
#include <QMutex>
#include <QTemporaryFile>
#include <QTimer>
#include <QUuid>
#include <memory>

class ClipPropertiesController;
class ProjectFolder;
class ProjectSubClip;
class QDomElement;

namespace Mlt {
class Producer;
class Properties;
} // namespace Mlt

/**
 * @class ProjectClip
 * @brief Represents a clip in the project (not timeline).
 * It will be displayed as a bin item that can be dragged onto the timeline.
 * A single bin clip can be inserted several times on the timeline, and the ProjectClip
 * keeps track of all the ids of the corresponding ClipModel.
 * Note that because of a limitation in melt and AvFilter, it is currently difficult to
 * mix the audio of two producers that are cut from the same master producer
 * (that produces small but noticeable clicking artifacts)
 * To workaround this, we need to have a master clip for each instance of the audio clip in the timeline. This class is tracking them all. This track also holds
 * a master clip for each clip where the timewarp producer has been applied
 */
class ProjectClip : public AbstractProjectItem, public ClipController
{
    Q_OBJECT

public:
    friend class Bin;
    friend bool TimelineModel::checkConsistency(const std::vector<int> &guideSnaps); // for testing
    /**
     * @brief Constructor; used when loading a project and the producer is already available.
     */
    static std::shared_ptr<ProjectClip> construct(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model,
                                                  std::shared_ptr<Mlt::Producer> &producer);
    /**
     * @brief Constructor.
     * @param description element describing the clip; the "kdenlive:id" attribute and "resource" property are used
     */
    static std::shared_ptr<ProjectClip> construct(const QString &id, const QDomElement &description, const QIcon &thumb,
                                                  std::shared_ptr<ProjectItemModel> model);

    /**
     * @brief Retreive original clip from proxy path when using external proxies
     */
    static const QString getOriginalFromProxy(QString proxyPath);
    /**
     * @brief Retreive original clip from proxy path when using external proxies
     */
    static const QString getProxyFromOriginal(QString originalPath);
    /**
     * @brief Retreive the list of external files used for sequence clip timewarp
     */
    QStringList timelineSequenceExtraResources() const;

    /**
     * @brief True if we started a clip reload
     */
    bool isReloading;

    struct TimeWarpInfo
    {
        bool enableRemap;
        QString timeMapData;
        int pitchShift;
        QString imageMode;
    };

protected:
    ProjectClip(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model, std::shared_ptr<Mlt::Producer> &producer);
    ProjectClip(const QString &id, const QDomElement &description, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model);

public:
    ~ProjectClip() override;

    void reloadProducer(bool refreshOnly = false, bool isProxy = false, bool forceAudioReload = false) override;

    /** @brief Returns a unique hash identifier used to store clip thumbnails. */
    // virtual void hash() = 0;

    /** @brief Returns this if @param id matches the clip's id or nullptr otherwise. */
    std::shared_ptr<ProjectClip> clip(const QString &id) override;

    std::shared_ptr<ProjectFolder> folder(const QString &id) override;

    std::shared_ptr<ProjectSubClip> getSubClip(int in, int out);

    /** @brief Returns this if @param ix matches the clip's index or nullptr otherwise. */
    std::shared_ptr<ProjectClip> clipAt(int ix) override;

    /** @brief Returns the clip type as defined in definitions.h */
    ClipType::ProducerType clipType() const override;

    /** @brief Get the marker model for this clip */
    std::shared_ptr<MarkerListModel> markerModel();

    /** @brief Set rating on item */
    void setRating(uint rating) override;

    bool selfSoftDelete(Fun &undo, Fun &redo) override;

    Fun getAudio_lambda() override;

    /** @brief Returns true if item has both audio and video enabled. */
    bool hasAudioAndVideo() const override;

    /** @brief Check if clip has a parent folder with id id */
    bool hasParent(const QString &id) const;

    /** @brief Returns true is the clip can have the requested state */
    bool isCompatible(PlaylistState::ClipState state) const;

    ClipPropertiesController *buildProperties(QWidget *parent);
    QPoint zone() const override;

    /** @brief Returns whether this clip has a url (=describes a file) or not. */
    bool hasUrl() const;

    /** @brief Returns the clip's url. */
    const QString url() const;

    /** @brief Returns the clip's frame size. */
    const QSize frameSize() const;

    /** @brief Returns the clip's duration. */
    GenTime duration() const;
    size_t frameDuration() const;

    /** @brief Returns the original clip's fps. */
    double getOriginalFps() const;

    bool rename(const QString &name, int column) override;

    QDomElement toXml(QDomDocument &document, bool includeMeta = false, bool includeProfile = true) override;

    const QVariant getData(DataType type) const override;

    QPixmap thumbnail(int width, int height);

    /** @brief Returns this clip's producer. */
    std::unique_ptr<Mlt::Producer> getThumbProducer() override;

    /** @brief Recursively disable/enable bin effects. */
    void setBinEffectsEnabled(bool enabled) override;

    /** @brief Set properties on this clip. TODO: should we store all in MLT or use extra m_properties ?. */
    void setProperties(const QMap<QString, QString> &properties, bool refreshPanel = false);

    /** @brief Get an XML property from MLT produced xml. */
    static QString getXmlProperty(const QDomElement &producer, const QString &propertyName, const QString &defaultValue = QString());

    QString getToolTip() const override;

    /** @brief The clip hash created from the clip's resource. */
    const QString hash(bool createIfEmpty = true);
    /** @brief The clip hash created from the clip's resource, plus the video stream in case of multi-stream clips. */
    const QString hashForThumbs();
    /** @brief Callculate a file hash from a path. */
    static const QPair<QByteArray, qint64> calculateHash(const QString &path);

    /** Cache for every audio Frame with 10 Bytes */
    /** format is frame -> channel ->bytes */
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
    const QString getAudioThumbPath(int stream);
    /** @brief Returns true if this producer has audio and can be splitted on timeline*/
    bool isSplittable() const;

    /** @brief Returns true if a clip corresponding to this bin is inserted in a timeline.
        Note that this function does not account for children, use TreeItem::accumulate if you want to get that information as well.
    */
    bool isIncludedInTimeline() override;
    /** @brief Returns a list of all timeline clip ids for this bin clip */
    QList<int> timelineInstances(QUuid activeUuid = QUuid()) const;
    QMap<QUuid, QList<int>> getAllTimelineInstances() const;
    /** @brief This function returns a cut to the master producer associated to the timeline clip with given ID.
        Each clip must have a different master producer (see comment of the class)
    */
    std::shared_ptr<Mlt::Producer> getTimelineProducer(int trackId, int clipId, PlaylistState::ClipState st, int audioStream = -1, double speed = 1.0,
                                                       bool secondPlaylist = false, const TimeWarpInfo timeremapInfo = {});

    /** @brief This function should only be used at loading. It takes a producer that was read from mlt, and checks whether the master producer is already in
       use. If yes, then we must create a new one, because of the mixing bug. In any case, we return a cut of the master that can be used in the timeline The
       bool returned has the following sementic:
           - if true, then the returned cut still possibly has effect on it. You need to rebuild the effectStack based on this
           - if false, then the returned cut don't have effects anymore (it's a fresh one), so you need to reload effects from the old producer
    */
    std::pair<std::shared_ptr<Mlt::Producer>, bool> giveMasterAndGetTimelineProducer(int clipId, std::shared_ptr<Mlt::Producer> master, PlaylistState::ClipState state, int tid, bool secondPlaylist = false);

    std::shared_ptr<Mlt::Producer> cloneProducer(bool removeEffects = false, bool timelineProducer = false);
    void cloneProducerToFile(const QString &path, bool thumbsProducer = false);
    static std::shared_ptr<Mlt::Producer> cloneProducer(const std::shared_ptr<Mlt::Producer> &producer);
    std::unique_ptr<Mlt::Producer> softClone(const char *list);
    /** @brief Returns a clone of the producer, useful for movit clip jobs
     */
    std::unique_ptr<Mlt::Producer> getClone();
    /** @brief Saves the subclips data as json
     */
    void updateZones();
    /** @brief Display Bin thumbnail given a percent
     */
    void getThumbFromPercent(int percent, bool storeFrame = false);
    /** @brief Get the frame position used for Bin clip thumbnail
     */
    int getThumbFrame() const;
    /** @brief Return audio cache for a stream
     */
    const QVector <uint8_t> audioFrameCache(int stream = -1);
    /** @brief Return FFmpeg's audio stream index for an MLT audio stream index
     */
    int getAudioStreamFfmpegIndex(int mltStream);
    void setClipStatus(FileStatus::ClipStatus status) override;
    /** @brief Rename an audio stream for this clip
     */
    void renameAudioStream(int id, const QString &name) override;

    /** @brief Add an audio effect on a specific audio stream with undo/redo. */
    void requestAddStreamEffect(int streamIndex, const QString effectName) override;
    /** @brief Add an audio effect on a specific audio stream for this clip. */
    void addAudioStreamEffect(int streamIndex, const QString effectName);
    /** @brief Remove an audio effect on a specific audio stream with undo/redo. */
    void requestRemoveStreamEffect(int streamIndex, const QString effectName) override;
    /** @brief Remove an audio effect on a specific audio stream for this clip. */
    void removeAudioStreamEffect(int streamIndex, QString effectName);
    /** @brief Get the list of audio stream effects for a defined stream. */
    QStringList getAudioStreamEffect(int streamIndex) const override;
    /** @brief Calculate the folder's hash (based on the files it contains). */
    static const QByteArray getFolderHash(const QDir &dir, QString fileName);
    /** @brief Check if the clip is included in timeline and reset its occurrences on producer reload. */
    void updateTimelineOnReload();
    int getRecordTime();
    /** @brief Return maximum audio level for a stream. */
    int getAudioMax(int stream);
    /** @brief A timeline clip was modified, reload its other timeline instances. */
    void reloadTimeline(std::shared_ptr<EffectStackModel> stack = nullptr);
    /** @brief Copy sequence clip timewarp producers to a new location (when saving / rendering). */
    void copyTimeWarpProducers(const QDir sequenceFolder, bool copy);
    /** @brief Refresh zones of insertion in timeline. */
    void refreshBounds();
    /** @brief Retuns a list of important enforces parameters in MLT format, for example to disable autorotate. */
    const QStringList enforcedParams() const;
    /** @brief Remove clip references in a timeline. */
    void purgeReferences(const QUuid &activeUuid, bool deleteClip = true);
    /** @brief Check if clip is referenced in a timeline, and return the clip's bin id if it is */
    const QString isReferenced(const QUuid &activeUuid) const;
    const QString baseThumbPath();
    /** @brief Returns false if the clip is or embeds a timeline with uuid. */
    bool canBeDropped(const QUuid &uuid) const;
    const QList<QUuid> registeredUuids() const;
    /** @brief Get the sequence's unique identifier, empty if not a sequence clip. */
    const QUuid &getSequenceUuid() const;
    void resetSequenceThumbnails();
    /** @brief Returns the clip name (usually file name) */
    QString clipName();
    /** @brief Save an xml playlist of current clip with in/out points as zone.x()/y() */
    void saveZone(QPoint zone, const QDir &dir);
    /** @brief When a sequence clip has a track change, update info and properties panel */
    void refreshTracksState(int tracksCount = -1);
    /**
     * @brief Returns a pixmap created from a frame of the producer.
     * @param position frame position
     * @param width width of the pixmap (only a guidance)
     * @param height height of the pixmap (only a guidance)
     */
    QPixmap pixmap(int position = 0, int width = 0, int height = 0);
    /** @brief Returns true if this clip has a variable framerate */
    bool hasVariableFps();

protected:
    friend class ClipModel;
    /** @brief This is a call-back called by a ClipModel when it is created
        @param timeline ptr to the pointer in which this ClipModel is inserted
        @param clipId id of the inserted clip
     */
    void registerTimelineClip(std::weak_ptr<TimelineModel> timeline, int clipId);
    void registerService(std::weak_ptr<TimelineModel> timeline, int clipId, const std::shared_ptr<Mlt::Producer> &service, bool forceRegister = false);

    /** @brief update the producer to reflect new parent folder */
    void updateParent(std::shared_ptr<TreeItem> parent) override;

    /** @brief This is a call-back called by a ClipModel when it is deleted
        @param clipId id of the deleted clip
    */
    void deregisterTimelineClip(int clipId, bool audioClip, const QUuid &uuid);
    void replaceInTimeline();
    void connectEffectStack() override;

public Q_SLOTS:
    /** @brief Store the audio thumbnails once computed. Note that the parameter is a value and not a reference, fill free to use it as a sink (use std::move to
     * avoid copy). */
    void updateAudioThumbnail(bool cachedThumb);
    /** @brief Delete the proxy file */
    void deleteProxy(bool reloadClip = true);
    /** @brief A clip job progressed, update display */
    void updateJobProgress();

    /** @brief Sets thumbnail for this clip. */
    void setThumbnail(const QImage &, int in, int out, bool inCache = false);

    /** @brief A proxy clip is available or disabled, update path and reload */
    void updateProxyProducer(const QString &path);

    /** @brief Request updating some clip droles */
    void updateTimelineClips(const QVector<int> &roles);

    /** @brief If a clip is invalid on load, mark it as such so we don't try to re-insert it on undo/redo. */
    void setInvalid();

    /**
     * Imports effect from a given producer
     * @param producer Producer containing the effects
     * @param originalDecimalPoint Decimal point to convert to “.”; See AssetParameterModel
     */
    void importEffects(const std::shared_ptr<Mlt::Producer> &producer, const QString &originalDecimalPoint = QString());

    /** @brief Sets the MLT producer associated with this clip
     *  @param producer The producer
     *  @param replaceProducer If true, we replace existing producer with this one
     *  @returns true if producer was changed
     * . */
    bool setProducer(std::shared_ptr<Mlt::Producer> producer, bool generateThumb = false, bool clearTrackProducers = true);

    void importJsonMarkers(const QString &json);
    /** @brief Refresh zones of insertion in timeline. */
    void checkClipBounds();

private:
    /** @brief Generate and store file hash if not available. */
    const QString getFileHash();
    QMutex m_producerMutex;
    QMutex m_thumbMutex;
    QByteArray m_thumbXml;
    const QString geometryWithOffset(const QString &data, int offset);
    QMap <QString, QByteArray> m_audioLevels;
    /** @brief If true, all timeline occurrences of this clip will be replaced from a fresh producer on reload. */
    bool m_resetTimelineOccurences;

    /** @brief This is a helper function that creates the disabled producer. This is a clone of the original one, with audio and video disabled */
    void createDisabledMasterProducer();
    QMap<QUuid, QList<int>> m_registeredClipsByUuid;
    QTimer m_boundaryTimer;

    /** @brief the following holds a producer for each audio clip in the timeline
     * keys are the id of the clips in the timeline, values are their values */
    std::unordered_map<int, std::shared_ptr<Mlt::Producer>> m_audioProducers;
    std::unordered_map<int, std::shared_ptr<Mlt::Producer>> m_videoProducers;
    std::unordered_map<int, std::shared_ptr<Mlt::Producer>> m_timewarpProducers;
    std::shared_ptr<Mlt::Producer> m_disabledProducer;
    // A temporary uuid used to reset thumbnails on producer change
    QUuid m_uuid;
    // The sequence unique identifier
    QUuid m_sequenceUuid;
    QTemporaryFile m_sequenceThumbFile;
    /** @brief Update the clip description from the properties. */
    void updateDescription();

Q_SIGNALS:
    void producerChanged(const QString &, Mlt::Producer prod);
    void refreshPropertiesPanel();
    void refreshAnalysisPanel();
    void refreshClipDisplay();
    void thumbReady(int, const QImage &);
    /** @brief Clip is ready, load properties. */
    void loadPropertiesPanel();
    void audioThumbReady();
    void updateStreamInfo(int ix);
    void boundsChanged(QVector <QPoint>bounds);
    void registeredClipChanged();
};
