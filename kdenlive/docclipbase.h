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
#include <kurl.h>
#include <qobject.h>
#include <qvaluevector.h>

#include "gentime.h"

class DocTrackBase;
class ClipManager;
class DocClipAVFile;

class DocClipBase : public QObject {
	Q_OBJECT
public:
	/** this enum determines the types of "feed" available within this clip. types must be non-exclusive
	 * - e.g. if you can have audio and video seperately, it should be possible to combin the two, as is
	 *   done here. If a new clip type is added then it should be possible to combine it with both audio
	 *   and video. */	
	enum CLIPTYPE { AUDIO = 1, VIDEO = 2, AV = 3};

	DocClipBase();
	virtual ~DocClipBase();

	/** sets the name of this clip. */
	void setName(const QString name);

	/** returns the name of this clip. */
	const QString &name() const;

	/** Sets the description for this clip. */
	void setDescription(const QString &descripton);
	
	/** Returns the description of this clip. */
	const QString &description() const;

	/** returns the duration of this clip */
	virtual GenTime duration() const = 0;

	/** Returns a url to a file describing this clip. Exactly what this url is,
	whether it is temporary or not, and whether it provokes a render will
	depend entirely on what the clip consists of. */
	virtual const KURL &fileURL() const = 0;

	/** Returns true if the clip duration is known, false otherwise. */
	virtual bool durationKnown() = 0;
	// Returns the number of frames per second that this clip should play at.
	virtual double framesPerSecond() const = 0;

	virtual bool isDocClipAVFile() const { return false; }
	virtual DocClipAVFile *toDocClipAVFile() { return 0; }
	/** Returns true if this clip is a project clip, false otherwise. Overridden in DocClipProject,
	 * where it returns true. */
	virtual bool isProjectClip() { return false; }
	
	// Appends scene times for this clip to the passed vector.
	virtual void populateSceneTimes(QValueVector<GenTime> &toPopulate) = 0;
	
	/** Reads in the element structure and creates a clip out of it.*/
	// Returns an XML document that describes part of the current scene.
	virtual QDomDocument sceneToXML(const GenTime &startTime, const GenTime &endTime) = 0;
	/** returns a QString containing all of the XML data required to recreate this clip. */
	virtual QDomDocument toXML();
	/** Returns a scene list generated from this clip. */
	virtual QDomDocument generateSceneList() = 0;
	/** Returns true if the xml passed matches the values in this clip */
	virtual bool matchesXML(const QDomElement &element) = 0;

	void addReference() { ++m_refcount; }
	void removeReference() { --m_refcount; }
	uint numReferences() const { return m_refcount; }

	/** Returns true if this clip has a meaningful filesize. */
	virtual bool hasFileSize() const = 0;

	/** Returns the filesize, or 0 if there is no appropriate filesize. */
	virtual uint fileSize() const = 0;

	/** Returns true if this clip refers to the clip passed in. A clip refers to another clip if
	 * it uses it as part of it's own composition. */
	virtual bool referencesClip(DocClipBase *clip) const = 0;

	static DocClipBase *createClip(ClipManager &clipManager, const QDomElement &element);
private: // Private attributes
	/** The name of this clip */
	QString m_name;
	/** A description of this clip */
	QString m_description;
	/** The number of times this clip is used in the project - the number of references to this clip
	 * that exist. */
	uint m_refcount;
};

#endif
