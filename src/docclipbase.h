/***************************************************************************
                          docclipbase.h  -  description
                             -------------------
    begin                : Fri Apr 12 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DOCCLIPBASE_H
#define DOCCLIPBASE_H

/**DocClip is a class for the various types of clip
  *@author Jason Wood
  */

#include <qdom.h>
#include <QPixmap>
#include <QObject>
#include <QTimer>
#include <QProcess>
#include <QFuture>

#include <KUrl>

#include "gentime.h"
#include "definitions.h"

/*
class DocTrackBase;
class DocClipAVFile;
class DocClipTextFile;
class DocClipVirtual;
class EffectDescriptionList;*/
class KdenliveDoc;
class KThumb;
class ClipManager;

namespace Mlt
{
class Producer;
};

struct CutZoneInfo {
    QPoint zone;
    QString description;
};


class DocClipBase: public QObject
{
Q_OBJECT public:
    /** this enum determines the types of "feed" available within this clip. types must be non-exclusive
     * - e.g. if you can have audio and video separately, it should be possible to combin the two, as is
     *   done here. If a new clip type is added then it should be possible to combine it with both audio
     *   and video. */

    DocClipBase(ClipManager *clipManager, QDomElement xml, const QString &id);
//    DocClipBase & operator=(const DocClipBase & clip);
    virtual ~ DocClipBase();

    /** returns the name of this clip. */
    const QString name() const;

    /** Returns the description of this clip. */
    const QString description() const;
    /** Does this clip need a transparent background (e.g. for titles). */
    bool isTransparent() const;

    /** Returns any property of this clip. */
    const QString getProperty(const QString &prop) const;
    void setProperty(const QString &key, const QString &value);
    void clearProperty(const QString &key);

    /** Returns the internal unique id of the clip. */
    const QString &getId() const;

    bool hasAudioThumb() const;
    //KThumb *thumbCreator;
    bool audioThumbCreated() const;
    /*void getClipMainThumb();*/

    /** returns the duration of this clip */
    const GenTime & duration() const;
    const GenTime maxDuration() const;
    /** returns the duration of this clip */
    void setDuration(GenTime dur);

    /** returns clip type (audio, text, image,...) */
    const CLIPTYPE & clipType() const;
    /** set clip type (audio, text, image,...) */
    void setClipType(CLIPTYPE type);

    /** remove tmp file if the clip has one (for example text clips) */
    void removeTmpFile() const;

    /** Returns a url to a file describing this clip. Exactly what this url is,
    whether it is temporary or not, and whether it provokes a render will
    depend entirely on what the clip consists of. */
    KUrl fileURL() const;

    /** Returns true if the clip duration is known, false otherwise. */
    bool durationKnown() const;
    // Returns the number of frames per second that this clip should play at.
    double framesPerSecond() const;

    bool isDocClipAVFile() const {
        return false;
    }

    /** Sets producers for the current clip (one for each track due to a limitation in MLT's track mixing */
    void setProducer(Mlt::Producer *producer, bool reset = false, bool readPropertiesFromProducer = false);
    /** Retrieve a producer for a track */
    Mlt::Producer *getProducer(int track = -1);
    /** Get a copy of the producer, for use in the clip monitor */
    Mlt::Producer *getCloneProducer();
    /** Retrieve the producer that shows only video */
    Mlt::Producer *videoProducer(int track);
    /** Retrieve the producer that shows only audio */
    Mlt::Producer *audioProducer(int track);

    /** Returns true if this clip is a project clip, false otherwise. Overridden in DocClipProject,
     * where it returns true. */
    bool isProjectClip() const {
        return false;
    }

    /** Reads in the element structure and creates a clip out of it.*/
    // Returns an XML document that describes part of the current scene.
    QDomDocument sceneToXML(const GenTime & startTime,
                            const GenTime & endTime) const;
    /** returns a QString containing all of the XML data required to recreate this clip. */
    QDomElement toXML(bool hideTemporaryProperties = false) const;

    /** Returns true if the xml passed matches the values in this clip */
    bool matchesXML(const QDomElement & element) const;

    void addReference() {
        ++m_refcount;
    }
    void removeReference() {
        --m_refcount;
    }
    uint numReferences() const {
        return m_refcount;
    }
    /** Returns true if this clip has a meaningful filesize. */
    bool hasFileSize() const;

    /** Returns the filesize, or 0 if there is no appropriate filesize. */
    qulonglong fileSize() const;

    /** Returns true if this clip refers to the clip passed in. A clip refers to another clip if
     * it uses it as part of it's own composition. */
    bool referencesClip(DocClipBase * clip) const;

    /** Returns the thumbnail producer used by this clip */
    KThumb *thumbProducer();

    /** Cache for every audio Frame with 10 Bytes */
    /** format is frame -> channel ->bytes */
    QMap<int, QMap<int, QByteArray> > m_audioFrameCache;

    /** Free cache data */
    void slotClearAudioCache();
    QString getClipHash() const;
    void refreshThumbUrl();
    const char *producerProperty(const char *name) const;
    void setProducerProperty(const char *name, const char *data);
    void resetProducerProperty(const char *name);
    void deleteProducers();

    /** Set default play zone for clip monitor */
    void setZone(QPoint zone);
    /** Get default play zone for clip monitor */
    QPoint zone() const;

    /** Returns true is clip is missing but user wants to keep it as placeholder */
    bool isPlaceHolder() const;
    void setValid();
    static QString getHash(const QString &path);

    void addCutZone(int in, int out, QString desc = QString());
    bool hasCutZone(QPoint p) const;
    void removeCutZone(int in, int out);
    QList <CutZoneInfo> cutZones() const;
    void updateCutZone(int oldin, int oldout, int in, int out, QString desc = QString());

    bool hasVideoCodec(const QString &codec) const;
    bool hasAudioCodec(const QString &codec) const;
    bool checkHash() const;
    void setPlaceHolder(bool place);
    QImage extractImage(int frame, int width, int height);
    void clearThumbProducer();
    void reloadThumbProducer();
    void cleanupProducers();
    bool isClean() const;
    bool getAudioThumbs();

private:   // Private attributes

    /** The number of times this clip is used in the project - the number of references to this clip
     * that exist. */
    uint m_refcount;
    QList <Mlt::Producer *> m_baseTrackProducers;
    QList <Mlt::Producer *> m_videoTrackProducers;
    QList <Mlt::Producer *> m_audioTrackProducers;
    QList <Mlt::Producer *> m_toDeleteProducers;
    CLIPTYPE m_clipType;

    /** A list of snap markers; these markers are added to a clips snap-to points, and are displayed as necessary. */
    QList < CommentedTime > m_snapMarkers;
    GenTime m_duration;

    KThumb *m_thumbProd;
    bool m_audioThumbCreated;

    /** a unique numeric id */
    QString m_id;

    /** Wheter the clip is a placeholder (clip missing but user wants to see it) */
    bool m_placeHolder;

    QList <CutZoneInfo> m_cutZones;

    void setAudioThumbCreated(bool isDone);
    /** Holds clip infos like fps, size,... */
    QMap <QString, QString> m_properties;
    /** Holds clip metadata like author, copyright,... */
    QMap <QString, QString> m_metadata;
    
    /** Try to make sure we don't delete a producer while using it */
    QMutex m_producerMutex;
    QMutex m_replaceMutex;
    
    /** @brief This timer will trigger creation of audio thumbnails. */
    QTimer m_audioTimer;

    /** Create connections for audio thumbnails */
    void slotRefreshProducer();
    void setProducerProperty(const char *name, int data);
    void setProducerProperty(const char *name, double data);
    void getFileHash(const QString &url);
    /** @brief When duplicating a producer, make sure all manually set properties are passed to it. */
    void adjustProducerProperties(Mlt::Producer *prod, const QString &id, bool mute, bool blind);
    /** @brief Create another instance of a producer. */
    Mlt::Producer *cloneProducer(Mlt::Producer *source);

   
public slots:
    void updateAudioThumbnail(const audioByteArray& data);
    QList < CommentedTime > commentedSnapMarkers() const;
    GenTime findNextSnapMarker(const GenTime & currTime);
    GenTime findPreviousSnapMarker(const GenTime & currTime);
    GenTime hasSnapMarkers(const GenTime & time);
    QString deleteSnapMarker(const GenTime & time);
    void editSnapMarker(const GenTime & time, QString comment);
    void addSnapMarker(const GenTime & time, QString comment);
    QList < GenTime > snapMarkers() const;
    QString markerComment(GenTime t);
    void setClipThumbFrame(const uint &ix);
    uint getClipThumbFrame() const;
    void setProperties(QMap <QString, QString> properties);
    void setMetadata(QMap <QString, QString> properties);
    /** Returns all current properties for this clip */
    QMap <QString, QString> properties() const;
    /** Return the current values for a set of properties */
    QMap <QString, QString> currentProperties(QMap <QString, QString> props);
    QMap <QString, QString> metadata() const;
    void slotExtractImage(QList <int> frames);

signals:
    void gotAudioData();
    /** @brief Generate a proxy clip (lower resolution copy) named like the clip's hash. */
    void createProxy(const QString &id);
    /** @brief Abort creation of the proxy clip (lower resolution copy). */
    void abortProxy(const QString &id, const QString &proxyPath);
};

#endif
