/***************************************************************************
                          docclipproject.h  -  description
                             -------------------
    begin                : Thu Jun 20 2002
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

#ifndef DOCCLIPPROJECT_H
#define DOCCLIPPROJECT_H

#include "docclipbase.h"
#include "doctrackbaselist.h"
#include "gentime.h"

class KdenliveDoc;

/**This "clip" consists of a number of tracks, clips, overlays, transitions and effects. It is basically
 capable of making multiple clips accessible as if they were a single clip. The "clipType()" of this clip
  depends entirely upon it's contents. */

class DocClipProject : public DocClipBase  {
	Q_OBJECT
public: 
	DocClipProject(KdenliveDoc *doc);
	~DocClipProject();

	GenTime duration() const;
  
	/** No descriptions */
	KURL fileURL();

	virtual QDomDocument toXML();
	/** Returns true if the clip duration is known, false otherwise. */
	virtual bool durationKnown();

	/** Returns the frames per second this clip runs at. */
	virtual int framesPerSecond() const;
	
  	/** Adds a track to the project */
  	void addTrack(DocTrackBase *track);
	int trackIndex(DocTrackBase *track);
	uint numTracks() const;
	DocTrackBase * findTrack(DocClipBase *clip);
	DocTrackBase * track(int track);
	bool moveSelectedClips(GenTime startOffset, int trackOffset);
	/** Returns a scene list generated from this clip. */
	virtual QDomDocument generateSceneList();

	/** Generates the tracklist for this clip from the xml fragment passed in.*/
	void generateTracksFromXML(const QDomElement &e);

  	/** Itterates through the tracks in the project. This works in the same way
	* as QPtrList::next(), although the underlying structures may be different. */
	DocTrackBase * nextTrack();
  	/** Returns the first track in the project, and resets the itterator to the first track.
	*This effectively is the same as QPtrList::first(), but the underyling implementation
	* may change. */
	DocTrackBase * firstTrack();

	/** If this is a project clip, return true. Overidden, always true from here. */	
	virtual bool isProjectClip() { return true; }
	
	/** Creates a clip from the passed QDomElement. This only pertains to those details
	 *  specific to DocClipProject.*/
	static DocClipProject * createClip(KdenliveDoc *doc, const QDomElement element);
	// Returns a list of times that this clip must break upon.
	virtual QValueVector<GenTime> sceneTimes();
	// Returns an XML document that describes part of the current scene.
	virtual QDomDocument sceneToXML(const GenTime &startTime, const GenTime &endTime);
signals:
  	/** This signal is emitted whenever tracks are added to or removed from the project. */
  	void trackListChanged();
	/** Emitted whenever the clip layout changes.*/
	void clipLayoutChanged();
	/** Emitted whenever a clip gets selected. */
	void signalClipSelected(DocClipBase *);
private:
	/** Blocks all track signals if block==true, or unblocks them otherwise. Use when you want
	 *  to temporarily ignore emits from tracks. */
	void blockTrackSignals(bool block);
	/** Returns the number of tracks in this project */
	void connectTrack(DocTrackBase *track);
  	/** The number of frames per second. */
  	int m_framesPerSecond;
  	/** Holds a list of all tracks in the project. */
  	DocTrackBaseList m_tracks;
};

#endif
