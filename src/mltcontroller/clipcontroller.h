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

#ifndef CLIPCONTROLLER_H
#define CLIPCONTROLLER_H

#include "definitions.h"

#include <QDateTime>
#include <QDir>
#include <QMutex>
#include <QObject>
#include <QString>
#include <memory>
#include <mlt++/Mlt.h>

class QPixmap;
class Bin;
class AudioStreamInfo;
class EffectStackModel;
class MarkerListModel;

/**
 * @class ClipController
 * @brief Provides a convenience wrapper around the project Bin clip producers.
 * It also holds a QList of track producers for the 'master' producer in case we
 * need to update or replace them
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
    explicit ClipController(const QString id, std::shared_ptr<Mlt::Producer> producer = nullptr);

public:
    virtual ~ClipController();

    /** @brief Returns true if the master producer is valid */
    bool isValid();

    /** @brief Stores the file's creation time */
    QDateTime date;

    /** @brief Replaces the master producer and (TODO) the track producers with an updated producer, for example a proxy */
    void updateProducer(const std::shared_ptr<Mlt::Producer> &producer);

    void getProducerXML(QDomDocument &document, bool includeMeta = false);

    /** @brief Returns a clone of our master producer. Delete after use! */
    Mlt::Producer *masterProducer();

    /** @brief Returns the clip name (usually file name) */
    QString clipName() const;

    /** @brief Returns the clip's description or metadata comment */
    QString description() const;

    /** @brief Returns the clip's MLT resource */
    const QString clipUrl() const;

    /** @brief Returns the clip's type as defined in definitions.h */
    ClipType clipType() const;

    /** @brief Returns the MLT's producer id */
    const QString binId() const;

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

    /**
     * @brief Returns a pixmap created from a frame of the producer.
     * @param position frame position
     * @param width width of the pixmap (only a guidance)
     * @param height height of the pixmap (only a guidance)
     */
    QPixmap pixmap(int position = 0, int width = 0, int height = 0);

    /** @brief Returns the MLT producer's service. */
    QString serviceName() const;

    /** @brief Returns the original master producer. */
    std::shared_ptr<Mlt::Producer> originalProducer();

    /** @brief Holds index of currently selected master clip effect. */
    int selectedEffectIndex;
    /** @brief Get a clone of master producer for a specific track. Retrieve it if it already exists
     *  in our list, otherwise we create it.
     *  Deprecated, track logic should be handled in timeline/track.cpp */
    Q_DECL_DEPRECATED Mlt::Producer *getTrackProducer(const QString &trackName, PlaylistState::ClipState clipState = PlaylistState::Original,
                                                      double speed = 1.0);

    /** @brief Sets the master producer for this clip when we build the controller without master clip. */
    void addMasterProducer(const std::shared_ptr<Mlt::Producer> &producer);

    /* @brief Returns the marker model associated with this clip */
    std::shared_ptr<MarkerListModel> getMarkerModel() const;

    void setZone(const QPoint &zone);
    QPoint zone() const;
    bool hasLimitedDuration() const;
    void forceLimitedDuration();
    Mlt::Properties &properties();
    void initEffect(QDomElement &xml);
    void addEffect(QDomElement &xml);
    bool copyEffect(std::shared_ptr<EffectStackModel> stackModel, int rowId);
    void removeEffect(int effectIndex, bool delayRefresh = false);
    EffectsList effectList();
    /** @brief Enable/disable an effect. */
    void changeEffectState(const QList<int> &indexes, bool disable);
    void updateEffect(const QDomElement &e, int ix);
    /** @brief Returns true if the bin clip has effects */
    bool hasEffects() const;
    /** @brief Returns info about clip audio */
    const std::unique_ptr<AudioStreamInfo> &audioInfo() const;
    /** @brief Returns true if audio thumbnails for this clip are cached */
    bool m_audioThumbCreated;
    /** @brief When replacing a producer, it is important that we keep some properties, for exemple force_ stuff and url for proxies
     * this method returns a list of properties that we want to keep when replacing a producer . */
    static const char *getPassPropertiesList(bool passLength = true);
    /** @brief Disable all Kdenlive effects on this clip */
    void setBinEffectsEnabled(bool enabled);
    /** @brief Create a Kdenlive EffectList xml data from an MLT service */
    static EffectsList xmlEffectList(Mlt::Profile *profile, Mlt::Service &service);
    /** @brief Returns the number of Kdenlive added effects for this bin clip */
    int effectsCount();
    /** @brief Move an effect in stack for this bin clip */
    void moveEffect(int oldPos, int newPos);
    /** @brief Save an xml playlist of current clip with in/out points as zone.x()/y() */
    void saveZone(QPoint zone, const QDir &dir);

    /* @brief This is the producer that serves as a placeholder while a clip is being loaded. It is created in Core at startup */
    static std::shared_ptr<Mlt::Producer> mediaUnavailable;

    /** @brief Returns a ptr to the effetstack associated with this element */
    std::shared_ptr<EffectStackModel> getEffectStack() const;

    /** @brief Append an effect to this producer's effect list */
    void addEffect(const QString &effectId);

protected:
    virtual void emitProducerChanged(const QString &, const std::shared_ptr<Mlt::Producer> &){};
    virtual void connectEffectStack(){};

    std::shared_ptr<Mlt::Producer> m_masterProducer;
    Mlt::Properties *m_properties;
    bool m_usesProxy;
    std::unique_ptr<AudioStreamInfo> m_audioInfo;
    QString m_service;
    QString m_path;
    int m_audioIndex;
    int m_videoIndex;
    ClipType m_clipType;
    bool m_hasLimitedDuration;
    QMutex m_effectMutex;
    void getInfoForProducer();
    // void rebuildEffectList(ProfileInfo info);
    std::shared_ptr<EffectStackModel> m_effectStack;
    std::shared_ptr<MarkerListModel> m_markerModel;

private:
    QMutex m_producerLock;
    QString m_controllerBinId;
};

#endif
