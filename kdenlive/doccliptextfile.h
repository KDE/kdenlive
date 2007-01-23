/***************************************************************************
                          doccliptextfile.h  -  description
                             -------------------
    begin                : Jan 31 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DOCCLIPTEXTFILE_H
#define DOCCLIPTEXTFILE_H

#include <qstring.h>
#include <qdom.h>

#include "gentime.h"
#include "docclipbase.h"

class ClipManager;

/**
	* Encapsulates a video, audio, picture, title, or any other kind of file that Kdenlive can support.
	* Each type of file should be encapsulated in it's own class, which should inherit this one.
  *@author Jason Wood
  */

class DocClipTextFile:public DocClipBase {
  Q_OBJECT public:

    /* text file clip */
    DocClipTextFile(const QString & name, const KURL & url, uint id);

    /* String clip */
  DocClipTextFile(const QString & name, const QString & text,
                  const GenTime & duration, const QDomDocument &xml, KURL url, const QPixmap &pix, bool transparency, uint id);


    DocClipTextFile(QDomDocument node);

     DocClipTextFile(const KURL & url);
    ~DocClipTextFile();
    QString fileName();
    double aspectRatio() const;

	/** Returns the filesize of the underlying avfile. */
    virtual uint fileSize() const;

	/** Returns the number of references to the underlying avfile. */
    uint numReferences() const;

	/** Returns the duration of the file */
    const GenTime & duration() const;


	/** Set clip duration */
    void setDuration(const GenTime & duration);

	/** Set url for clip */
    void setFileURL(const KURL & url);

	/** Returns the type of this clip */
    const DocClipBase::CLIPTYPE & clipType() const;

    virtual void removeTmpFile() const;

    QDomDocument toXML() const;
	/** Returns the url of the AVFile this clip contains */
    
    const KURL & fileURL() const;
	/** Creates a clip from the passed QDomElement. This only pertains to those details specific
	 *  to DocClipTextFile. This action should only occur via the clipManager.*/
    static DocClipTextFile *createClip(const QDomElement element);
	/** Returns true if the clip duration is known, false otherwise. */
    virtual bool durationKnown() const;
    // Returns the number of frames per second that this clip should play at.
    virtual double framesPerSecond() const;

    const QDomDocument & textClipXml() const;
    void setTextClipXml(const QDomDocument &xml);

    //returns clip video properties -reh
    uint clipWidth() const;
    uint clipHeight() const;
    // Appends scene times for this clip to the passed vector.
    virtual void populateSceneTimes(QValueVector < GenTime >
	&toPopulate) const;

    virtual QDomDocument generateSceneList(bool addProducers = true, bool rendering = false) const;

    // Returns an XML document that describes part of the current scene.
    virtual QDomDocument sceneToXML(const GenTime & startTime,
	const GenTime & endTime) const;

	/** Returns true if this clip refers to the clip passed in. For a DocClipTextFile, this
	 * is true if this == clip */
    virtual bool referencesClip(DocClipBase * clip) const;

	/** Returns true if this clip has a meaningful filesize. */
    virtual bool hasFileSize() const {
	return true;
    }
	/** returns true if the xml passed matches the values in the clip */
	virtual bool matchesXML(const QDomElement & element) const;

    virtual bool isDocClipTextFile() const {
	return true;
    } 
    
    virtual DocClipTextFile *toDocClipTextFile() {
	return this;
  }

    virtual DocClipVirtual *toDocClipVirtual() {
	return 0;
  }
  
  virtual bool isDocClipAVFile() const {
      return false;
  } 

  virtual bool isDocClipVirtual() const {
      return false;
  } 
    
  virtual DocClipAVFile *toDocClipAVFile() {
      return 0;
  }
  
  bool isTransparent();
  void setAlpha(bool transp);
    
    private:
	/** A play object factory, used for calculating information, and previewing files */
	/** Determines whether this file contains audio, video or both. */
     DocClipBase::CLIPTYPE m_clipType;
	/** The duration of this file. */
    GenTime m_duration;
	/** Holds the url for this AV file. */
    KURL m_url;
	/** True if the duration of this AVFile is known, false otherwise. */
    bool m_durationKnown;
	/** The number of frames per second that this AVFile runs at. */
    double m_framesPerSecond;
	/** The size in bytes of this AVFile */
    uint m_filesize;
	/** a unique numeric id */
    uint m_id;
    QString m_color;
    QDomDocument m_xml;
    
    bool m_alphaTransparency;

    //extended video file properties -reh
    uint m_height;
    uint m_width;
};

#endif
