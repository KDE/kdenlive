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
#include <arts/kmedia2.h>

class KdenliveDoc;

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

	/** Returns where this clip starts on the track (seconds element)*/
  	long trackStartSeconds();
	/** Returns where this clip starts on the track (ms element)*/
	long trackStartMs();
	/** Returns where this clip starts in ms (seconds * 1000) + ms */
	long trackStart();
	/** Sets the position that this clip resides upon it's track. */
	void setTrackStart(long seconds, long ms);
	void setTrackStart(long ms);

	/** sets the name of this clip. */
	void setName(QString name);

	/** returns the name of this clip. */
	QString name();

	/** set the cropStart time for this clip.The "crop" timings are those which define which
	part of a clip is wanted in the edit. For example, a clip may be 60 seconds long, but the first
	10 is not needed. Setting the "crop start time" to 10 seconds means that the first 10 seconds isn't
	used. The crop times are necessary, so that if at later time you decide you need an extra second
	at the beginning of the clip, you can re-add it.*/
	void setCropStartTime(long ms);

	/** returns the cropStart time for this clip */ 
	long cropStartTime();

	/** set the cropDuration time for this clip. */	
	void setCropDuration(long ms);

	/** returns the cropDuration time for this clip. */
	long cropDuration();
  
	/** returns the seconds element of the duration of this clip */
	long durationSeconds();
	/** returns the Milliseconds element of the duration of this clip */
	long durationMs();
	/** returns a QString containing all of the XML data required to recreate this clip. */
	virtual QDomDocument toXML();
	
	/** returns the duration of this clip in milliseconds */
	virtual long duration() = 0;
	/** Returns a url to a file describing this clip. Exactly what this url is,
	whether it is temporary or not, and whether it provokes a render will
	depend entirely on what the clip consists of. */
	virtual KURL fileURL() = 0;

	/** Reads in the element structure and creates a clip out of it. */
	static DocClipBase *createClip(KdenliveDoc &doc, QDomElement element);
  /** returns whether or not this clip is selected. */
  bool isSelected();
  /** Sets whether this clip is selected or not */
  void setSelected(bool state=true);
private: // Private attributes
	/** The name of this clip */
	QString m_name;
	/** Where this clip starts on the track that it resides on. */
	Arts::poTime m_trackStart;
	/** The cropped start time for this clip - e.g. if the clip is 10 seconds long, this might say that the
	 * the bit we want starts 3 seconds in.
	 **/
	Arts::poTime m_cropStart;
	/** The cropped duration for this clip. Determines exactly how much of the clip from the m_cropStart
	 * time that we actually want.
	 **/
	Arts::poTime m_cropDuration;
  /** This variable becomes true when a clip has become selected. */
  bool m_selected;
};

#endif
