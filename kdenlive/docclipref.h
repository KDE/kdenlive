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

#ifndef DOCCLIPREF_H
#define DOCCLIPREF_H

/**DocClip is a class for the various types of clip
  *@author Jason Wood
  */

#include <qdom.h>
#include <kurl.h>
#include <qobject.h>
#include <qvaluevector.h>

#include "gentime.h"

class ClipManager;
class DocClipBase;
class DocTrackBase;
class KdenliveDoc;

class DocClipRef : public QObject {
	Q_OBJECT
public:
	DocClipRef(DocClipBase *clip);
	~DocClipRef();

	/** Returns where this clip starts */
	const GenTime &trackStart() const;
	/** Sets the position that this clip resides upon it's track. */
	void setTrackStart(const GenTime time);

	/** returns the name of this clip. */
	const QString &name() const;

	/** Returns the description of this clip. */
	const QString &description() const;

	/** set the cropStart time for this clip.The "crop" timings are those which define which
	part of a clip is wanted in the edit. For example, a clip may be 60 seconds long, but the first
	10 is not needed. Setting the "crop start time" to 10 seconds means that the first 10 seconds isn't
	used. The crop times are necessary, so that if at later time you decide you need an extra second
	at the beginning of the clip, you can re-add it.*/
	void setCropStartTime(const GenTime &);

	/** returns the cropStart time for this clip */ 
	const GenTime &cropStartTime() const;

	/** set the trackEnd time for this clip. */
	void setTrackEnd(const GenTime &time);

	/** returns the cropDuration time for this clip. */
	GenTime cropDuration() const;

	/** returns a QString containing all of the XML data required to recreate this clip. */
	QDomDocument toXML() const;

	/** Returns true if the XML data matches the contexts of the clipref. */
	bool matchesXML(const QDomElement &element) const;

	/** returns the duration of this clip */
	GenTime duration() const;
	/** Returns a url to a file describing this clip. Exactly what this url is,
	whether it is temporary or not, and whether it provokes a render will
	depend entirely on what the clip consists of. */
	const KURL &fileURL() const;

	/** Reads in the element structure and creates a clip out of it.*/
	static DocClipRef *createClip(ClipManager &clipManager, const QDomElement &element);
	/** Sets the parent track for this clip. */
	void setParentTrack(DocTrackBase *track, const int trackNum);
	/** Returns the track number. This is a hint as to which track the clip is on, or
	 * should be placed on. */
	int trackNum() const;
	/** Returns the end of the clip on the track. A convenience function, equivalent
	to trackStart() + cropDuration() */
	GenTime trackEnd() const;
	/** Returns the parentTrack of this clip. */
	DocTrackBase * parentTrack();
	/** Move the clips so that it's trackStart coincides with the time specified. */
	void moveTrackStart(const GenTime &time);
	/** Returns an identical but seperate (i.e. "deep") copy of this clip. */
	DocClipRef * clone(ClipManager &clipManager);
	/** Returns true if the clip duration is known, false otherwise. */
	bool durationKnown() const;
	// Returns the number of frames per second that this clip should play at.
	double framesPerSecond() const;
	/** Returns a scene list generated from this clip. */
	QDomDocument generateSceneList();
	/** Returns true if this clip is a project clip, false otherwise. Overridden in DocClipProject,
	 * where it returns true. */
	bool isProjectClip() { return false; }

	// Appends scene times for this clip to the passed vector.
	void populateSceneTimes(QValueVector<GenTime> &toPopulate);

	// Returns an XML document that describes part of the current scene.
	QDomDocument sceneToXML(const GenTime &startTime, const GenTime &endTime);

	bool referencesClip(DocClipBase *clip) const;

	/** Returns the number of times the DocClipBase referred to is referenced - by both this clip
	 * and other clips. */
	uint numReferences() const;

	/** Returns true if this clip has a meaningful filesize. */
	bool hasFileSize() const;
	/** Returns the filesize, or 0 if there is no appropriate filesize. */
	uint fileSize() const;

	/** TBD - figure out a way to make this unnecessary. */
	DocClipBase *referencedClip() { return m_clip; }

	/** Returns a vector containing the snap marker, in track time rather than clip time. */
	QValueVector<GenTime> snapMarkersOnTrack() const;

	void addSnapMarker(const GenTime &time);
	void deleteSnapMarker(const GenTime &time);

private: // Private attributes
	void setSnapMarkers(QValueVector<GenTime> markers);

	/** Where this clip starts on the track that it resides on. */
	GenTime m_trackStart;
	/** The cropped start time for this clip - e.g. if the clip is 10 seconds long, this
	 * might say that the the bit we want starts 3 seconds in.
	 **/
	GenTime m_cropStart;
	/** The end time of this clip on the track.
	 **/
	GenTime m_trackEnd;
	/** The track to which this clip is parented. If NULL, the clip is not
	parented to any track. */
	DocTrackBase * m_parentTrack;
	/** The number of this track. This is the number of the track the clip resides on.
	It is possible for this to be set and the parent track to be 0, in this situation
	m_trackNum is a hint as to where the clip should be place when it get's parented
	to a track. */
	int m_trackNum;

	/** The clip to which this clip refers. */
	DocClipBase *m_clip;

	KdenliveDoc *m_document;

	/** A list of snap markers; these markers are added to a clips snap-to points, and are displayed as necessary. */
	QValueVector<GenTime> m_snapMarkers;
};

#endif
