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

#include "gentime.h"
#include "docclipbase.h"

class AVFile;

class KdenliveDoc;

/**
	* Encapsulates a video, audio, picture, title, or any other kind of file that Kdenlive can support.
	* Each type of file should be encapsulated in it's own class, which should inherit this one.
  *@author Jason Wood
  */

class DocClipAVFile : public DocClipBase {
	Q_OBJECT
public:
	DocClipAVFile(KdenliveDoc *doc, const QString &name, const KURL &url);
	DocClipAVFile(KdenliveDoc *doc, AVFile *avFile);
	~DocClipAVFile();
	QString fileName();
	
	/** Returns the duration of the file */
	GenTime duration() const;	
	/** Returns the type of this clip */
	DocClipBase::CLIPTYPE clipType();

	QDomDocument toXML();
	/** Returns the url of the AVFile this clip contains */
	KURL fileURL();
	/** Creates a clip from the passed QDomElement. This only pertains to those details specific to DocClipAVFile.*/
	static DocClipAVFile * createClip(KdenliveDoc *doc, const QDomElement element);
	/** Returns true if the clip duration is known, false otherwise. */
	virtual bool durationKnown();
	virtual int framesPerSecond() const;
	/** Returns a scene list generated from this clip. */
	virtual QDomDocument generateSceneList();
	// Returns a list of times that this clip must break upon.
	virtual QValueVector<GenTime> sceneTimes();
	// Returns an XML document that describes part of the current scene.
	virtual QDomDocument sceneToXML(const GenTime &startTime, const GenTime &endTime);

	virtual bool containsAVFile(AVFile *file);
private:	
	/** A play object factory, used for calculating information, and previewing files */
	/** Determines whether this file contains audio, video or both. */
	DocClipBase::CLIPTYPE m_clipType;
	/** Holds a pointer to an AVFile which contains details of the file this clip portrays. */
	AVFile * m_avfile;
};

#endif

