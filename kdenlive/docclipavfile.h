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

class ClipManager;

/**
	* Encapsulates a video, audio, picture, title, or any other kind of file that Kdenlive can support.
	* Each type of file should be encapsulated in it's own class, which should inherit this one.
  *@author Jason Wood
  */

class DocClipAVFile : public DocClipBase {
	Q_OBJECT
public:
	DocClipAVFile(const QString &name, const KURL &url);
	DocClipAVFile(const KURL &url);
	~DocClipAVFile();
	QString fileName();

	/** Calculates properties for the file, including the size of the file, the duration of the file,
	 * the file format, etc.
	 **/
	void calculateFileProperties(const QMap<QString, QString> &attributes);

	/** Returns the filesize of the underlying avfile. */
	virtual uint fileSize() const;

	/** Returns the number of references to the underlying avfile. */
	uint numReferences() const;

	/** Returns the duration of the file */
	const GenTime &duration() const;
	/** Returns the type of this clip */
	DocClipBase::CLIPTYPE clipType();

	QDomDocument toXML() const;
	/** Returns the url of the AVFile this clip contains */
	const KURL &fileURL() const;
	/** Creates a clip from the passed QDomElement. This only pertains to those details specific
	 *  to DocClipAVFile. This action should only occur via the clipManager.*/
	static DocClipAVFile * createClip(const QDomElement element);
	/** Returns true if the clip duration is known, false otherwise. */
	virtual bool durationKnown() const;
	virtual double framesPerSecond() const;
	// Appends scene times for this clip to the passed vector.
	virtual void populateSceneTimes(QValueVector<GenTime> &toPopulate) const;
	// Returns an XML document that describes part of the current scene.
	virtual QDomDocument sceneToXML(const GenTime &startTime, const GenTime &endTime) const;
	/** Returns true if this clip refers to the clip passed in. For a DocClipAVFile, this
	 * is true if this == clip */
	virtual bool referencesClip(DocClipBase *clip) const;

	/** Returns true if this clip has a meaningful filesize. */
	virtual bool hasFileSize() const { return true; }

	/** returns true if the xml passed matches the values in the clip */
	virtual bool matchesXML(const QDomElement &element) const;

	virtual bool isDocClipAVFile() const { return true; }
	virtual DocClipAVFile *toDocClipAVFile() { return this; }

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
};

#endif

