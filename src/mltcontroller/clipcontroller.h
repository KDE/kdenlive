/*
SPDX-FileCopyrightText: 2012 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "undohelper.hpp"

#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <QString>
#include <QReadWriteLock>
#include <memory>
#include <mlt++/Mlt.h>

class QPixmap;
class Bin;
class AudioStreamInfo;
class EffectStackModel;
class MarkerListModel;
class MarkerSortModel;

/** @class ClipController
 *  @brief Provides a convenience wrapper around the project Bin clip producers.
 *  It also holds a QList of track producers for the 'master' producer in case we
 *  need to update or replace them
 */
class ClipController
{
public:
    friend class Bin;
    /**
     * @brief Constructor.
     The constructor is protected because you should call the static Construct instead
     * @param bincontroller reference to the bincontroller
     * @param producer producer to create reference to
     */
    explicit ClipController(const QString &id, const std::shared_ptr<Mlt::Producer> &producer = nullptr);

public:
    virtual ~ClipController();

    QMutex producerMutex;

    /** @brief Returns true if the master producer is valid */
    bool isValid();

    /** @brief Returns true if the source file exists */
    bool sourceExists() const;

    /** @brief Stores the file's creation time */
    QDateTime date;

    /** @brief Replaces the master producer and (TODO) the track producers with an updated producer, for example a proxy */
    void updateProducer(const std::shared_ptr<Mlt::Producer> &producer);
    static const QByteArray producerXml(Mlt::Producer producer, bool includeMeta, bool includeProfile);

    void getProducerXML(QDomDocument &document, bool includeMeta = false, bool includeProfile = true);

    /** @brief Returns a clone of our master producer. Delete after use! */
    Mlt::Producer *masterProducer();

    /** @brief Returns the clip's MLT resource */
    const QString clipUrl() const;

    /** @brief Returns the clip's type as defined in definitions.h */
    ClipType::ProducerType clipType() const;

    /** @brief Returns the MLT's producer id */
    const QString binId() const;

    /** @brief Returns this clip's producer. */
    virtual std::unique_ptr<Mlt::Producer> getThumbProducer() = 0;

    virtual void reloadProducer(bool refreshOnly = false, bool isProxy = false, bool forceAudioReload = false) = 0;

    /** @brief Rename an audio stream. */
    virtual void renameAudioStream(int id, const QString &name) = 0;

    /** @brief Add an audio effect on a specific audio stream for this clip. */
    virtual void requestAddStreamEffect(int streamIndex, const QString effectName) = 0;
    /** @brief Remove an audio effect on a specific audio stream for this clip. */
    virtual void requestRemoveStreamEffect(int streamIndex, const QString effectName) = 0;
    virtual QStringList getAudioStreamEffect(int streamIndex) const = 0;

    /** @brief Returns the clip's duration */
    GenTime getPlaytime() const;
    int getFramePlaytime() const;
    /**
     * @brief Sets a property.
     * @param name name of the property
     * @param value the new value
     */
    void setProducerProperty(const QString &name, const QString &value);
    void setProducerProperty(const QString &name, int value);
    void setProducerProperty(const QString &name, double value);
    /** @brief Reset a property on the MLT producer (=delete the property). */
    void resetProducerProperty(const QString &name);

    /**
     * @brief Returns the list of all properties starting with prefix. For subclips, the list is of this type:
     * { subclip name , subclip in/out } where the subclip in/ou value is a semi-colon separated in/out value, like "25;220"
     */
    QMap<QString, QString> getPropertiesFromPrefix(const QString &prefix, bool withPrefix = false);

    /**
     * @brief Returns the value of a property.
     * @param name name o the property
     */
    QMap<QString, QString> currentProperties(const QMap<QString, QString> &props);
    bool hasProducerProperty(const QString &name) const;
    QString getProducerProperty(const QString &key) const;
    int getProducerIntProperty(const QString &key) const;
    qint64 getProducerInt64Property(const QString &key) const;
    QColor getProducerColorProperty(const QString &key) const;
    double getProducerDoubleProperty(const QString &key) const;

    double originalFps() const;
    QString videoCodecProperty(const QString &property) const;
    const QString codec(bool audioCodec) const;
    const QString getClipHash() const;
    const QSize getFrameSize() const;
    /** @brief Returns the clip duration as a string like 00:00:02:01. */
    const QString getStringDuration();
    int getProducerDuration() const;
    char *framesToTime(int frames) const;

    /** @brief Returns the MLT producer's service. */
    QString serviceName() const;

    /** @brief Returns the original master producer. */
    std::shared_ptr<Mlt::Producer> originalProducer();

    /** @brief Holds index of currently selected master clip effect. */
    int selectedEffectIndex;

    /** @brief Sets the master producer for this clip when we build the controller without master clip. */
    void addMasterProducer(const std::shared_ptr<Mlt::Producer> &producer);

    /** @brief Returns the marker model associated with this clip */
    std::shared_ptr<MarkerListModel> getMarkerModel() const;
    std::shared_ptr<MarkerSortModel> getFilteredMarkerModel() const;

    void setZone(const QPoint &zone);
    bool hasLimitedDuration() const;
    void forceLimitedDuration();
    Mlt::Properties &properties();
    void mirrorOriginalProperties(std::shared_ptr<Mlt::Properties> props);
    bool copyEffect(const std::shared_ptr<EffectStackModel> &stackModel, int rowId);
    bool copyEffectWithUndo(const std::shared_ptr<EffectStackModel> &stackModel, int rowId, Fun &undo, Fun &redo);
    /** @brief Returns true if the bin clip has effects */
    bool hasEffects() const;
    /** @brief Returns true if the clip contains at least one audio stream */
    bool hasAudio() const;
    /** @brief Returns true if the clip contains at least one video stream */
    bool hasVideo() const;
    /** @brief Returns the default state a clip should be in. If the clips contains both video and audio, this defaults to video */
    PlaylistState::ClipState defaultState() const;
    /** @brief Returns info about clip audio */
    const std::unique_ptr<AudioStreamInfo> &audioInfo() const;
    /** @brief Returns true if audio thumbnails for this clip are cached */
    bool m_audioThumbCreated;
    /** @brief When replacing a producer, it is important that we keep some properties, for example force_ stuff and url for proxies
     * this method returns a list of properties that we want to keep when replacing a producer . */
    static const char *getPassPropertiesList(bool passLength = true);
    /** @brief Disable all Kdenlive effects on this clip */
    void setBinEffectsEnabled(bool enabled);
    /** @brief Returns the number of Kdenlive added effects for this bin clip */
    int effectsCount();
    /** @brief Returns all urls of external files used by effects on this bin clip (e.g. LUTs)*/
    QStringList filesUsedByEffects();

    /** @brief This is the producer that serves as a placeholder while a clip is being loaded. It is created in Core at startup */
    static std::shared_ptr<Mlt::Producer> mediaUnavailable;

    /** @brief Returns a ptr to the effetstack associated with this element */
    std::shared_ptr<EffectStackModel> getEffectStack() const;

    /** @brief Append an effect to this producer's effect list */
    bool addEffect(const QString &effectId, stringMap params = {});

    /** @brief Returns the list of all audio streams indexes for this clip */
    QMap <int, QString> audioStreams() const;
    /** @brief Returns the absolute audio stream index (usable for MLT's astream property) */
    int audioStreamIndex(int stream) const;
    /** @brief Returns the number of channels per audio stream. */
    QList <int> activeStreamChannels() const;
    /** @brief Returns the list of active audio streams indexes for this clip */
    QMap <int, QString> activeStreams() const;
    /** @brief Returns the list of active audio streams ffmpeg indexes for this clip */
    QVector<int> activeFfmpegStreams() const;
    /** @brief Returns the count of audio streams for this clip */
    int audioStreamsCount() const;
    /** @brief Get the path to the original clip url (in case it is proxied) */
    const QString getOriginalUrl();
    /** @brief Returns true if we are using a proxy for this clip. */
    bool hasProxy() const;
    /** @brief Delete or re-assign all markers in a category. */
    bool removeMarkerCategories(QList<int> toRemove, const QMap<int, int> remapCategories, Fun &undo, Fun &redo);
    // This is the helper function that checks if the clip has audio and video and stores the result
    void checkAudioVideo();
    bool isFullRange() const;

protected:
    /** @brief Mutex to protect the producer properties on read/write */
    mutable QReadWriteLock m_producerLock;
    virtual void connectEffectStack(){};

    // Update audio stream info
    void refreshAudioInfo();
    void backupOriginalProperties();
    void clearBackupProperties();

    std::shared_ptr<Mlt::Producer> m_masterProducer;
    Mlt::Properties *m_properties;
    bool m_usesProxy;
    std::unique_ptr<AudioStreamInfo> m_audioInfo;
    QString m_service;
    QString m_path;
    int m_videoIndex;
    ClipType::ProducerType m_clipType;
    bool m_forceLimitedDuration;
    bool m_hasMultipleVideoStreams;
    QMutex m_effectMutex;
    void getInfoForProducer();
    // void rebuildEffectList(ProfileInfo info);
    std::shared_ptr<EffectStackModel> m_effectStack;
    std::shared_ptr<MarkerListModel> m_markerModel;
    std::shared_ptr<MarkerSortModel> m_markerFilterModel;
    bool m_hasAudio;
    bool m_hasVideo;
    QMap<int, QStringList> m_streamEffects;
    /** @brief Store clip url temporarily while the clip controller has not been created. */
    QString m_temporaryUrl;

private:
    /** @brief Temporarily store clip properties until producer is available */
    QMap <QString, QVariant> m_tempProps;
    QString m_controllerBinId;
    /** @brief Build the audio info object */
    void buildAudioInfo(int audioIndex);
};
