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

#include "gentime.h"

class KdenliveDoc;
class DocTrackBase;

class DocClipBase {
public:
	/** this enum determines the types of "feed" available within this clip. types must be non-exlcusive
	 * - e.g. if you can have audio and video seperately, it should be possible to combin the two, as is
	 *   done here. If a new clip type is added then it should be possible to combine it with both audio
	 *   and video. */	
	enum CLIPTYPE { AUDIO = 1,
			VIDEO = 2,
        		AV = 3};
	DocClipBase();
	virtual ~DocClipBase();

	/** Returns where this clip starts */
	const GenTime &trackStart() const;
	/** Sets the position that this clip resides upon it's track. */
	void setTrackStart(GenTime time);

	/** sets the name of this clip. */
	void setName(QString name);

	/** returns the name of this clip. */
	QString name();

	/** set the cropStart time for this clip.The "crop" timings are those which define which
	part of a clip is wanted in the edit. For example, a clip may be 60 seconds long, but the first
	10 is not needed. Setting the "crop start time" to 10 seconds means that the first 10 seconds isn't
	used. The crop times are necessary, so that if at later time you decide you need an extra second
	at the beginning of the clip, you can re-add it.*/
	void setCropStartTime(const GenTime &);

	/** returns the cropStart time for this clip */ 
	const GenTime &cropStartTime() const;

	/** set the cropDuration time for this clip. */	
	void setCropDuration(const GenTime &time);

	/** returns the cropDuration time for this clip. */
	GenTime cropDuration();
  
	/** returns a QString containing all of the XML data required to recreate this clip. */
	virtual QDomDocument toXML();
	
	/** returns the duration of this clip */
	virtual GenTime duration() = 0;
	/** Returns a url to a file describing this clip. Exactly what this url is,
	whether it is temporary or not, and whether it provokes a render will
	depend entirely on what the clip consists of. */
	virtual KURL fileURL() = 0;

	/** Reads in the element structure and creates a clip out of it. */
	static DocClipBase *createClip(KdenliveDoc &doc, QDomElement element);
  /** Sets the parent track for this clip. */
  void setParentTrack(DocTrackBase *track, int trackNum);
  /** Returns the track number. This is a hint as to which track the clip is on, or should be placed on. */
  int trackNum();
  /** Returns the end of the clip on the track. A convenience function, equivalent
to trackStart() + cropDuration() */
  GenTime trackEnd();
  /** Returns the parentTrack of this clip. */
  DocTrackBase * parentTrack();
private: // Private attributes
	/** The name of this clip */
	QString m_name;
	/** Where this clip starts on the track that it resides on. */
	GenTime m_trackStart;
	/** The cropped start time for this clip - e.g. if the clip is 10 seconds long, this might say that the
	 * the bit we want starts 3 seconds in.
	 **/
	GenTime m_cropStart;
	/** The cropped duration for this clip. Determines exactly how much of the clip from the m_cropStart
	 * time that we actually want.
	 **/
	GenTime m_cropDuration;
  /** The track to which this clip is parented. If NULL, the clip is not
parented to any track. */
  DocTrackBase * m_parentTrack;
  /** The number of this track. This is the number of the track the clip resides on.
It is possible for this to be set and the parent track to be 0, in this situation
m_trackNum is a hint as to where the clip should be place when it get's parented
to a track. */
  int m_trackNum;;
};

#endif
