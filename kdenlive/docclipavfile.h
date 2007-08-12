/***************************************************************************
                          avfile.h  -  description
                             -------------------
    begin                : Fri Feb 15 2002
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

#ifndef DOCCLIPAVFILE_H
#define DOCCLIPAVFILE_H

#include <qstring.h>
#include <qtimer.h>

#include "gentime.h"
#include "docclipbase.h"

class ClipManager;

/**
	* Encapsulates a video, audio, picture, title, or any other kind of file that Kdenlive can support.
	* Each type of file should be encapsulated in it's own class, which should inherit this one.
  *@author Jason Wood
  */

class DocClipAVFile:public DocClipBase {
  Q_OBJECT public:

    /* video/audio clip */
    DocClipAVFile(const QString & name, const KURL & url, uint id);

    /**  image clip  */
     DocClipAVFile(const KURL & url, const GenTime & duration, bool alphaTransparency, uint id);

    /**  slideshow clip  */
     DocClipAVFile(const KURL & url, const QString & extension,
                   const int &ttl, const GenTime & duration, bool alphaTransparency, bool loop, bool crossfade, const QString &lumaFile, double lumasoftness, uint lumaduration, uint id);

    /* color clip */
     DocClipAVFile(const QString & color, const GenTime & duration,
	uint id);


    DocClipAVFile(QDomDocument node);

     DocClipAVFile(const KURL & url);
    ~DocClipAVFile();
    QString fileName();

	/** Calculates properties for the file, including the size of the file, the duration of the file,
	 * the file format, etc.
	 **/
    void calculateFileProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata);
    double aspectRatio() const;

	/** Returns the filesize of the underlying avfile. */
    virtual uint fileSize() const;

    QString videoCodec() const;
    QString audioCodec() const;

	/** Returns the duration of the file */
    const GenTime & duration() const;

	/** Returns the duration of the file */
    const QString & color() const;

	/** Set clip duration */
    void setDuration(const GenTime & duration);

	/** Set color for color clip */
    void setColor(const QString color);

	/** Set url for clip */
    void setFileURL(const KURL & url);

	/** Returns the type of this clip */
    const DocClipBase::CLIPTYPE & clipType() const;

    virtual void removeTmpFile() const;

    QDomDocument toXML() const;
	/** Returns the url of the AVFile this clip contains */
    const KURL & fileURL() const;
	/** Creates a clip from the passed QDomElement. This only pertains to those details specific
	 *  to DocClipAVFile. This action should only occur via the clipManager.*/
    static DocClipAVFile *createClip(const QDomElement element);
	/** Returns true if the clip duration is known, false otherwise. */
    virtual bool durationKnown() const;
	/** If the clip is a playlist, returns a list of broken clip urls */
    QStringList brokenPlayList();

    void setAlpha(bool transp);
    void setCrossfade( bool cross);
    bool isTransparent() const;
    bool loop() const;
    void setLoop(bool isOn);
    bool hasCrossfade() const;
    int clipTtl() const;
    void setClipTtl(const int &ttl);
    void setLumaFile(const QString & luma);
    const QString & lumaFile() const;
    void setLumaSoftness(const double & softness);
    const double & lumaSoftness() const;
    void setLumaDuration(const uint & duration);
    const uint &lumaDuration() const;

    virtual double framesPerSecond() const;
    //returns clip video properties -reh
    virtual uint clipWidth() const;
    virtual uint clipHeight() const;
    virtual QString avDecompressor();
    virtual QString avSystem();
    //returns audio properties
    virtual uint audioChannels() const;
    virtual QString audioFormat();
    virtual uint audioBits() const;
    virtual uint audioFrequency() const;
    // Appends scene times for this clip to the passed vector.
    virtual void populateSceneTimes(QValueVector < GenTime >
	&toPopulate) const;

    virtual QDomDocument generateSceneList(bool addProducers = true, bool rendering = false) const;

    // Returns an XML document that describes part of the current scene.
    virtual QDomDocument sceneToXML(const GenTime & startTime,
	const GenTime & endTime) const;

	/** Returns true if this clip refers to the clip passed in. For a DocClipAVFile, this
	 * is true if this == clip */
    virtual bool referencesClip(DocClipBase * clip) const;

	/** Returns true if this clip has a meaningful filesize. */
    virtual bool hasFileSize() const {
	return true;
    }
	/** returns true if the xml passed matches the values in the clip */
    virtual bool matchesXML(const QDomElement & element) const;

    virtual bool isDocClipAVFile() const {
	return true;
    }

    virtual DocClipAVFile *toDocClipAVFile() {
	return this;
    } 

    virtual bool isDocClipTextFile() const {
      return false;
    } 

    virtual bool isDocClipVirtual() const {
      return false;
    }

    virtual DocClipTextFile *toDocClipTextFile() {
      return 0;
    }

    virtual DocClipVirtual *toDocClipVirtual() {
      return 0;
    }

    public slots:
	void getAudioThumbs();
	void stopAudioThumbs();
	QString formattedMetaData();
	void prepareThumbs() const;

    private:

	/** The duration of this file. */
    GenTime m_duration;
	/** Holds the url for this AV file. */
    KURL m_url;
	/** True if the duration of this AVFile is known, false otherwise. */
    bool m_durationKnown;
	/** The number of frames per second that this AVFile runs at. */
    double m_framesPerSecond;
	/** the background color for color clips */
    QString m_color;
	/** A play object factory, used for calculating information, and previewing files */
	/** Determines whether this file contains audio, video or both. */
     DocClipBase::CLIPTYPE m_clipType;


	/** The size in bytes of this AVFile */
    uint m_filesize;
    /** Should the background be transparent (for image / slideshow clips) */
    bool m_alphaTransparency;
    /** Should we crossfade between images (only for slideshows) */
    bool m_hasCrossfade;
    /** Should we loop through the images (only for slideshows) */
    bool m_loop;

    /** Holds file metadata (comment, copyright, author,...) */
    QMap < QString, QString > m_metadata;

    //extended video file properties -reh
    uint m_height;
    uint m_width;
    QString m_decompressor;
    QString m_system;
    //audio file properties
    uint m_channels;
    QString m_format;
    uint m_bitspersample;
    uint m_frequency;
    int m_ttl;
	/** The name of luma file transition for slideshows */
    QString m_luma;
    double m_lumasoftness;
    uint m_lumaduration;
    QString m_videoCodec;
    QString m_audioCodec;
    QTimer *m_audioTimer;
};

#endif
