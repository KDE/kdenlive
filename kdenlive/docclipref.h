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
#include <qtimer.h>

#include <kurl.h>
#include <klocale.h>
#include <qobject.h>
#include <qvaluevector.h>
#include <qmap.h>
#include <qptrvector.h>


#include "gentime.h"
#include "effectstack.h"
#include "docclipbase.h"
#include "kthumb.h"
#include "transitionstack.h"

class ClipManager;
class DocTrackBase;
class KdenliveDoc;
class EffectDescriptionList;
class Transition;

struct AudioIdentifier {
    double m_framenum;
    double m_numframes;
    double m_imageWidth;
    double m_imageHeight;
    int m_channel;

    bool operator==(const AudioIdentifier & rhs) const {
	bool matches = true;

	 matches &= m_framenum == rhs.m_framenum;
	 matches &= m_numframes == rhs.m_numframes;
	 matches &= m_imageWidth == rhs.m_imageWidth;
	 matches &= m_imageHeight == rhs.m_imageHeight;
	 matches &= m_channel == rhs.m_channel;

	 return matches;
    } 
    
    bool operator<(const AudioIdentifier & rhs) const {
	if (m_framenum < rhs.m_framenum)
	    return true;
	if (m_framenum > rhs.m_framenum)
	    return false;

	if (m_numframes < rhs.m_numframes)
	    return true;
	if (m_numframes > rhs.m_numframes)
	    return false;

	if (m_imageWidth < rhs.m_imageWidth)
	    return true;
	if (m_imageWidth > rhs.m_imageWidth)
	    return false;

	if (m_imageHeight < rhs.m_imageHeight)
	    return true;
	if (m_imageHeight > rhs.m_imageHeight)
	    return false;

	if (m_channel < rhs.m_channel)
	    return true;
	if (m_channel > rhs.m_channel)
	    return false;

	return false;
    }
};


class CommentedTime
    {
    public:
        CommentedTime(): t(GenTime(0)) {}
        CommentedTime( const GenTime time, QString comment)
            : t( time ), c( comment )
        { }

        QString comment()   const          { return (c.isEmpty() ? i18n("Marker") : c);}
        GenTime time() const          { return t; }
        void    setComment( QString comm) { c = comm; }
    private:
        GenTime t;
        QString c;
    };


class DocClipRef:public QObject {
  Q_OBJECT public:
    DocClipRef(DocClipBase * clip);
    ~DocClipRef();

	/** Returns where this clip starts */
    const GenTime & trackStart() const;
	/** Sets the position that this clip resides upon it's track. */
    void setTrackStart(const GenTime time);

	/** Returns the time where this clip is central on the track (i.e. trackStart + trackEnd / 2) Useful for grabbing a track time value
	that is definitely on the clip. */
    GenTime trackMiddleTime() const;

	/** returns the name of this clip. */
    const QString & name() const;

	/** Returns the description of this clip. */
    const QString & description() const;

	/** Sets the description of this clip */
    void setDescription(const QString & description);

	/** set the cropStart time for this clip.The "crop" timings are those which define which
	part of a clip is wanted in the edit. For example, a clip may be 60 seconds long, but the first
	10 is not needed. Setting the "crop start time" to 10 seconds means that the first 10 seconds isn't
	used. The crop times are necessary, so that if at later time you decide you need an extra second
	at the beginning of the clip, you can re-add it.*/
    void setCropStartTime(const GenTime &);
    void moveCropStartTime(const GenTime & time);

	/** returns the cropStart time for this clip */
    const GenTime & cropStartTime() const;

	/** set the trackEnd time for this clip. */
    void setTrackEnd(const GenTime & time);

	/** returns the cropDuration time for this clip. */
    GenTime cropDuration() const;
	/** Sets the cropDuration time for this clip - note, this will change the track end time as well. */
    void setCropDuration(const GenTime & time);

	/** returns a QString containing all of the XML data required to recreate this clip. */
    QDomDocument toXML() const;

	/** Returns true if the XML data matches the contexts of the clipref. */
    bool matchesXML(const QDomElement & element) const;

	/** returns the duration of this clip */
    const GenTime & duration() const;

	/** Returns a url to a file describing this clip. Exactly what this url is,
	whether it is temporary or not, and whether it provokes a render will
	depend entirely on what the clip consists of. */
    const KURL & fileURL() const;

	/** Reads in the element structure and creates a clip out of it.*/
    static DocClipRef *createClip(const EffectDescriptionList & effectList,
	ClipManager & clipManager, const QDomElement & element);
	/** Sets the parent track for this clip. */
    void setParentTrack(DocTrackBase * track, const int trackNum);
	/** Returns the track number. This is a hint as to which track the clip is on, or
	 * should be placed on. */
    int trackNum() const;
    
        /** Return the position of the track in MLT's playlist*/
    int playlistTrackNum() const;
    int playlistNextTrackNum() const;
    int playlistOtherTrackNum(int num) const;
    
	/** Returns the end of the clip on the track. A convenience function, equivalent
	to trackStart() + cropDuration() */
    GenTime trackEnd() const;
	/** Returns the parentTrack of this clip. */
    DocTrackBase *parentTrack();
	/** Move the clips so that it's trackStart coincides with the time specified. */
    void moveTrackStart(const GenTime & time);
	/** Returns an identical but seperate (i.e. "deep") copy of this clip. */
    DocClipRef *clone(const EffectDescriptionList & effectList, ClipManager & clipManager);
	/** Returns true if the clip duration is known, false otherwise. */
    bool durationKnown() const;
    // Returns the number of frames per second that this clip should play at.
    double framesPerSecond() const;
    //return clip video properties -reh
	/** Returns clip type (audio, video,...) */
    DocClipBase::CLIPTYPE clipType() const;
    uint clipWidth() const;
    uint clipHeight() const;
    QString avDecompressor();
    QString avSystem();
    //returns audio properties -reh
    uint audioChannels() const;
    QString audioFormat();
    uint audioBits() const;
    uint audioFrequency() const;
	/** Returns a scene list generated from this clip. */
    QDomDocument generateSceneList();
    QDomDocument generateXMLClip();
    QDomDocument generateOffsetXMLClip(GenTime start, GenTime end);
    QDomDocumentFragment generateOffsetXMLTransition(bool hideVideo, bool hideAudio, GenTime start, GenTime end);

	/** Returns true if this clip is a project clip, false otherwise. Overridden in DocClipProject,
	 * where it returns true. */
    bool isProjectClip() {
	return false;
    }
    // Appends scene times for this clip to the passed vector.
    void populateSceneTimes(QValueVector < GenTime > &toPopulate);

    // Returns an XML document that describes part of the current scene.
    QDomDocument sceneToXML(const GenTime & startTime,
	const GenTime & endTime);

    bool referencesClip(DocClipBase * clip) const;

	/** Returns the number of times the DocClipBase referred to is referenced - by both this clip
	 * and other clips. */
    uint numReferences() const;

	/** Returns true if this clip has a meaningful filesize. */
    bool hasFileSize() const;
	/** Returns the filesize, or 0 if there is no appropriate filesize. */
    uint fileSize() const;

	/** TBD - figure out a way to make this unnecessary. */
    DocClipBase *referencedClip() {
	return m_clip;
    }

	/** Returns a vector containing the snap marker in clip time */
    QValueVector < GenTime > snapMarkers()const;

	/** Returns a vector containing the snap marker, in track time rather than clip time. */
    QValueVector < GenTime > snapMarkersOnTrack()const;
    QValueVector < CommentedTime > commentedSnapMarkers()const;
    QValueVector < CommentedTime > commentedTrackSnapMarkers()const;

	/** Adds a snap marker at the given clip time (as opposed to track time) */
    void addSnapMarker(const GenTime & time, QString comment, bool notify = true);
	/** Adds a snap marker at the given clip time (as opposed to track time) */
    void editSnapMarker(const GenTime & time, QString comment);

	/** Deletes a snap marker at the given clip time (as opposed to track time) */
    void deleteSnapMarker(const GenTime & time);

	/** Returns true if this clip has a snap marker at the specified clip time */
    bool hasSnapMarker(const GenTime & time);

	/** Finds and returns the time of the snap marker directly before time. If there isn't one, returns 0. */
    GenTime findPreviousSnapMarker(const GenTime & time);

	/** Finds and returns the time of the snap marker directly after time. If there isn't one, returns the duration of the clip. */
    GenTime findNextSnapMarker(const GenTime & time);

	/** Adds an effect to the effect stack at the specified marker position. */
    void addEffect(uint index, Effect * effect);

	/** Adds an effect to effect stack at the specified marker position. */
    void deleteEffect(uint index);

	/** Sets the index for the currently selected effect */
    void setEffectStackSelectedItem(uint ix);

	/** Returns the currently selected effect */
    Effect *selectedEffect();

	/** Returns the effect stack */
    const EffectStack & effectStack() const {
	return m_effectStack;
    }
	/** Returns the effect with the given index. Return s0 and outputs error if index is out of range. */
	Effect *effectAt(uint index) const;

    int numEffects() const {
	return m_effectStack.count();
    } 
    void setEffectStack(const EffectStack & effectStack);

    const QPixmap & getAudioImage(int width, int height, double frame,
	double numFrames, int channel);

	/** Returns true if effects are applied on the clip */
    bool hasEffect();
	/** Returns a list of the clip effects names */
    QStringList clipEffectNames();

    void disconnectThumbCreator();

    Transition *transitionAt(const GenTime &time);

    void clearVideoEffects();

    QPixmap thumbnail(bool end = false);
    int thumbnailWidth();
    QTimer *startTimer;
    QTimer *endTimer;
    
  public slots:
	QByteArray getAudioThumbs(int channel,double frame, double frameLength, int arrayWidth);
        void updateThumbnail(int frame, QPixmap newpix);
        
        /** generate start and end thumbnails for the clip */
        void generateThumbnails();
        
        /** If a clip is a video, its thumbnails should be adjusted when resizing the clip. */ 
        bool hasVariableThumbnails();
        
        bool hasTransition(DocClipRef *clip);
        void deleteTransitions();
        void addTransition(Transition *transition);
	void deleteTransition(QDomElement transitionXml);
        TransitionStack clipTransitions();
        void resizeTransitionStart(uint ix, GenTime time);
        void resizeTransitionEnd(uint ix, GenTime time);
        void moveTransition(uint ix, GenTime time);
        QDomDocumentFragment generateXMLTransition(bool hideVideo, bool hideAudio);
	void updateAudioThumbnail(QMap<int,QMap<int,QByteArray> > data);
	void refreshAudioThumbnail();
	void setSpeed(double speed, double endspeed = 1.0);
	double speed() const;
	void refreshCurrentTrack();
	QValueVector < GenTime > transitionSnaps();

  private slots:
        /** Fetch the thumbnail for the clip start */
        void fetchStartThumbnail();
        /** Fetch the thumbnail for the clip end */
        void fetchEndThumbnail();
	GenTime adjustTimeToSpeed(GenTime t) const;

  private:			// Private attributes
    void setSnapMarkers(QValueVector < CommentedTime > markers);
    int getAudioPart(double from, double length,int channel);
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
    DocTrackBase *m_parentTrack;
	/** The number of this track. This is the number of the track the clip resides on.
	It is possible for this to be set and the parent track to be 0, in this situation
	m_trackNum is a hint as to where the clip should be place when it get's parented
	to a track. */
    int m_trackNum;

	/** The clip to which this clip refers. */
    DocClipBase *m_clip;

	/** A list of snap markers; these markers are added to a clips snap-to points, and are displayed as necessary. */
    QValueVector < CommentedTime > m_snapMarkers;

	/** A list of effects that operate on this and only this clip. */
    EffectStack m_effectStack;
    
        /** A list of transitions attached to this clip. */
    TransitionStack m_transitionStack;

    QMap < AudioIdentifier, QPixmap > m_audioMap;
    QPixmap m_thumbnail;
    QPixmap m_endthumbnail;
	/** Clip speed, used for slowmotion */
    double m_speed;
    double m_endspeed;
    
signals:
    void getClipThumbnail(KURL, int, int, int);
    //void getAudioThumbnails(KURL, int ,double,double ,int);
};

#endif
