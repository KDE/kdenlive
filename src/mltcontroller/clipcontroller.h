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

#include <mlt++/Mlt.h>
#include <QString>
#include <QObject>
#include <QUrl>
#include <QDateTime>

class QPixmap;
class BinController;
class AudioStreamInfo;

/**
 * @class ClipController
 * @brief Provides a convenience wrapper around the project Bin clip producers.
 * It also holds a QList of track producers for the 'master' producer in case we
 * need to update or replace them
 */


class ClipController : public QObject
{
public:
    /**
     * @brief Constructor.
     * @param bincontroller reference to the bincontroller
     * @param producer producer to create reference to
     */
    ClipController(BinController *bincontroller, Mlt::Producer &producer);
    /**
     * @brief Constructor used when opening a document and encountering a 
     * track producer before the master producer. The masterProducer MUST be set afterwards
     * @param bincontroller reference to the bincontroller
     */
    ClipController(BinController *bincontroller);
    virtual ~ClipController();
    
    /** @brief Returns true if the master producer is valid */
    bool isValid();
    
    /** @brief Stores the file's creation time */
    QDateTime date;

    /** @brief Replaces the master producer and (TODO) the track producers with an updated producer, for example a proxy */
    void updateProducer(const QString &id, Mlt::Producer *producer);

    void getProducerXML(QDomDocument& document);

    /** @brief Append a track producer retrieved from document loading to out list */
    void appendTrackProducer(const QString trackName, Mlt::Producer &producer);

    /** @brief Returns a clone of our master producer. Delete after use! */
    Mlt::Producer *masterProducer();
    Mlt::Producer *zoneProducer(int in, int out);

    /** @brief Returns the MLT's producer id */
    const QString clipId();

    /** @brief Returns the clip name (usually file name) */
    QString clipName() const;

    /** @brief Returns the clip's description or metadata comment */
    QString description() const;

    /** @brief Returns the clip's MLT resource */
    QUrl clipUrl() const;

    /** @brief Returns the clip's type as defined in definitions.h */
    ClipType clipType() const;

    /** @brief Returns the clip's duration */
    GenTime getPlaytime() const;
    /**
     * @brief Sets a property.
     * @param name name of the property
     * @param value the new value
     */
    void setProperty(const QString &name, const QString &value);
    void setProperty(const QString& name, int value);
    void setProperty(const QString& name, double value);
    void resetProperty(const QString& name);
    
    /**
     * @brief Returns the list of SubClips defined for this clip. the list is of this type:
     * { subclip name , subclip in/out } where the subclip in/ou value is a semi-colon separated in/out value, like "25;220"
     */
    QMap <QString, QString> getSubClips();
    
    /**
     * @brief Returns the value of a property.
     * @param name name o the property
     */
    QString property(const QString &name) const;
    int int_property(const QString &name) const;
    double double_property(const QString &name) const;
    QColor color_property(const QString &name) const;

    double originalFps() const;
    QString videoCodecProperty(const QString &property) const;
    const QString codec(bool audioCodec) const;
    QSize originalFrameSize() const;
    const QString getClipHash() const;

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
    Mlt::Producer &originalProducer();
    
    /** @brief Returns the current profile's display aspect ratio. */
    double dar() const;
    /** @brief Holds index of currently selected master clip effect. */
    int selectedEffectIndex;
    /** @brief Get a clone of master producer for a specific track. Retrieve it if it already exists
     *  in our list, otherwise we create it. 
     *  Deprecated, track logic should be handled in timeline/track.cpp */
    Q_DECL_DEPRECATED Mlt::Producer *getTrackProducer(const QString trackName, PlaylistState::ClipState clipState = PlaylistState::Original, double speed = 1.0);
    
    /** @brief Sets the master producer for this clip when we build the controller without master clip. */
    void addMasterProducer(Mlt::Producer &producer);
    
    QList < CommentedTime > commentedSnapMarkers() const;
    GenTime findNextSnapMarker(const GenTime & currTime);
    GenTime findPreviousSnapMarker(const GenTime & currTime);
    QString deleteSnapMarker(const GenTime & time);
    void editSnapMarker(const GenTime & time, const QString &comment);
    void addSnapMarker(const CommentedTime &marker);
    void loadSnapMarker(const QString &seconds, const QString &hash);
    QList < GenTime > snapMarkers() const;
    QString markerComment(const GenTime &t) const;
    CommentedTime markerAt(const GenTime &t) const;
    void setZone(const QPoint &zone);
    QPoint zone() const;
    bool hasLimitedDuration() const;
    Mlt::Properties &properties();
    void addEffect(const ProfileInfo pInfo, QDomElement &effect);
    void removeEffect(const ProfileInfo pInfo, int effectIndex);
    EffectsList effectList();
    /** @brief Enable/disable an effect. */
    void changeEffectState(const QList <int> indexes, bool disable);
    void updateEffect(const ProfileInfo pInfo, const QDomElement &old, const QDomElement &e, int ix);
    /** @brief Returns true if the bin clip has effects */
    bool hasEffects() const;
    /** @brief Returns info about clip audio */
    AudioStreamInfo *audioInfo() const;
    /** @brief Returns true if audio thumbnails for this clip are cached */
    bool audioThumbCreated;

private:
    Mlt::Producer *m_masterProducer;
    Mlt::Properties *m_properties;
    AudioStreamInfo *m_audioInfo;
    EffectsList m_effectList;
    QString m_service;
    QUrl m_url;
    int m_audioIndex;
    int m_videoIndex;
    ClipType m_clipType;
    bool m_hasLimitedDuration;
    BinController *m_binController;
    /** @brief A list of snap markers; these markers are added to a clips snap-to points, and are displayed as necessary. */
    QList < CommentedTime > m_snapMarkers;
    /** @brief When replacing a producer, it is important that we keep some properties, for exemple force_ stuff and url for proxies
     * this method returns a list of properties that we want to keep when replacing a producer . */
    const char *getPassPropertiesList() const;
    void getInfoForProducer();
    void rebuildEffectList();
};

#endif
