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

namespace Mlt {
class Producer;
};


class DocClipBase: public QObject {
Q_OBJECT public:
    /** this enum determines the types of "feed" available within this clip. types must be non-exclusive
     * - e.g. if you can have audio and video seperately, it should be possible to combin the two, as is
     *   done here. If a new clip type is added then it should be possible to combine it with both audio
     *   and video. */

    DocClipBase(ClipManager *clipManager, QDomElement xml, const QString &id);
//    DocClipBase & operator=(const DocClipBase & clip);
    virtual ~ DocClipBase();

    /** sets the name of this clip. */
    void setName(const QString name);

    /** returns the name of this clip. */
    const QString & name() const;

    /** Returns the description of this clip. */
    const QString description() const;
    /** Does this clip need a transparent background (e.g. for titles). */
    bool isTransparent() const;

    /** Returns any property of this clip. */
    const QString getProperty(const QString prop) const;
    void setProperty(const QString &key, const QString &value);
    void clearProperty(const QString &key);

    /** Returns the internal unique id of the clip. */
    const QString &getId() const;
    void setId(const QString &newId);

    //KThumb *thumbCreator;
    bool audioThumbCreated() const;
    /*void getClipMainThumb();*/

    /** returns the duration of this clip */
    const GenTime & duration() const;
    const GenTime &maxDuration() const;
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

    void setProducer(Mlt::Producer *producer);
    Mlt::Producer *producer();

    /*virtual DocClipAVFile *toDocClipAVFile() {
    return 0;
    }

    virtual DocClipTextFile *toDocClipTextFile() {
        return 0;
    }

    virtual bool isDocClipTextFile() const {
        return false;
    }

    virtual bool isDocClipVirtual() const {
        return false;
    }

    virtual DocClipVirtual *toDocClipVirtual() {
        return 0;
    }*/

    /** Returns true if this clip is a project clip, false otherwise. Overridden in DocClipProject,
     * where it returns true. */
    bool isProjectClip() const {
        return false;
    }
    // Appends scene times for this clip to the passed vector.
    /* virtual void populateSceneTimes(QList < GenTime >
     &toPopulate) const = 0;*/

    /** Reads in the element structure and creates a clip out of it.*/
    // Returns an XML document that describes part of the current scene.
    QDomDocument sceneToXML(const GenTime & startTime,
                            const GenTime & endTime) const;
    /** returns a QString containing all of the XML data required to recreate this clip. */
    QDomElement toXML() const;

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
    uint fileSize() const;

    /** Returns true if this clip refers to the clip passed in. A clip refers to another clip if
     * it uses it as part of it's own composition. */
    bool referencesClip(DocClipBase * clip) const;

    /** Sets the thumbnail to be used by this clip */
    void setThumbnail(const QPixmap & pixmap);

    /** Returns the thumbnail producer used by this clip */
    KThumb *thumbProducer();

    /** Returns the thumbnail used by this clip */
    const QPixmap & thumbnail() const;

    static DocClipBase *createClip(KdenliveDoc *doc, const QDomElement & element);
    /** Cache for every audio Frame with 10 Bytes */
    /** format is frame -> channel ->bytes */
    QMap<int, QMap<int, QByteArray> > audioFrameChache;

    /** Free cache data */
    void slotClearAudioCache();
    void askForAudioThumbs();

private:   // Private attributes
    /** The name of this clip */
    QString m_name;
    /** A description of this clip */
    QString m_description;
    /** The number of times this clip is used in the project - the number of references to this clip
     * that exist. */
    uint m_refcount;
    Mlt::Producer *m_clipProducer;
    CLIPTYPE m_clipType;

    /** A list of snap markers; these markers are added to a clips snap-to points, and are displayed as necessary. */
    QList < CommentedTime > m_snapMarkers;

    /** A thumbnail for this clip */
    QPixmap m_thumbnail;
    GenTime m_duration;

    QTimer *m_audioTimer;
    KThumb *m_thumbProd;
    bool m_audioThumbCreated;

    /** a unique numeric id */
    QString m_id;
    void setAudioThumbCreated(bool isDone);
    /** Holds clip infos like fps, size,... */
    QMap <QString, QString> m_properties;
    /** Create connections for audio thumbnails */
    void slotCreateAudioTimer();
    void slotRefreshProducer();

public slots:
    void updateAudioThumbnail(QMap<int, QMap<int, QByteArray> > data);
    bool slotGetAudioThumbs();
    QList < CommentedTime > commentedSnapMarkers() const;
    void setSnapMarkers(QList < CommentedTime > markers);
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
    QMap <QString, QString> properties() const;

signals:
    void getAudioThumbs();
    void gotAudioData();
};

#endif
