/***************************************************************************
                          docsubclip.h  -  description
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

#ifndef DOCSUBCLIP_H
#define DOCSUBCLIP_H

#include <qstring.h>

#include "gentime.h"
#include "docclipbase.h"

class KdenliveDoc;

/**
	* Encapsulates a video, audio, picture, title, or any other kind of file that Kdenlive can support.
	* Each type of file should be encapsulated in it's own class, which should inherit this one.
  *@author Jason Wood
  */
class DocSubClip:public DocClipBase {
  Q_OBJECT public:
    DocSubClip(KdenliveDoc * doc, DocClipBase * avFile);
    ~DocSubClip();

    QString fileName();

	/** Returns the duration of the file */
    const GenTime & duration() const;

	/** Returns the type of this clip */
     DocClipBase::CLIPTYPE clipType();

    QDomDocument toXML();
	/** Returns the url of the AVFile this clip contains */
    KURL fileURL();
	/** Creates a clip from the passed QDomElement. This only pertains to those details specific to DocSubClip.*/
    static DocSubClip *createClip(KdenliveDoc * doc,
	const QDomElement element);
	/** Returns true if the clip duration is known, false otherwise. */
    virtual bool durationKnown();
    virtual double framesPerSecond() const;
	/** Returns a scene list generated from this clip. */
    virtual QDomDocument generateSceneList(bool addProducers = true);
    // Appends scene times for this clip to the passed vector.
    virtual void populateSceneTimes(QValueVector < GenTime > &toPopulate);
    // Returns an XML document that describes part of the current scene.
    virtual QDomDocument sceneToXML(const GenTime & startTime,
	const GenTime & endTime);

    virtual bool matchesXML(const QDomElement & element);
    virtual bool hasFileSize() const {
	return false;
    } virtual uint fileSize() const {
	return 0;
    } virtual bool referencesClip(DocClipBase * clip) const;
  private:
	/** A play object factory, used for calculating information, and previewing files */
	/** Determines whether this file contains audio, video or both. */
     DocClipBase::CLIPTYPE m_clipType;
	/** Holds a pointer to the clip which contains details of the file this clip portrays. */
    DocClipBase *m_clip;
};

#endif
