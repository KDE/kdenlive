/***************************************************************************
                          docclipproject.cpp  -  description
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

#include "docclipproject.h"

#include <assert.h>
#include <qptrvector.h>
#include <qvaluevector.h>
#include <qptrlist.h>

#include <kcommand.h>
#include <kdebug.h>

#include "doctrackbase.h"
#include "kaddtransitioncommand.h"
#include "doctrackclipiterator.h"
#include "kdenlivesettings.h"


DocClipProject::DocClipProject(double framesPerSecond, int width, int height):DocClipBase(),
m_framesPerSecond(framesPerSecond), m_videowidth(width), m_videoheight(height)
{
    m_tracks.setAutoDelete(true);
}

DocClipProject::~DocClipProject()
{
}

void DocClipProject::requestProjectClose()
{
    DocClipRefList list;

    DocTrackBase *srcTrack;
    for (uint track = 0; track < numTracks(); ++track) {
	srcTrack = m_tracks.at(track);
	srcTrack->deleteClips(true);
	srcTrack->deleteClips(false);
    }
}

const GenTime & DocClipProject::duration() const
{
    return m_projectLength;
}

/** No descriptions */
const KURL & DocClipProject::fileURL() const
{
    static KURL emptyUrl;

    return emptyUrl;
}

void DocClipProject::slotClipReferenceChanged()
{
    emit clipReferenceChanged();
}

/** Returns true if the clip duration is known, false otherwise. */
bool DocClipProject::durationKnown() const
{
    return true;
}

//virtual
double DocClipProject::framesPerSecond() const
{
    return m_framesPerSecond;
}

void DocClipProject::setFramesPerSecond(double fps)
{
    m_framesPerSecond = fps;
}

/** Adds a track to the project */
void DocClipProject::slotAddTrack(DocTrackBase * track, int ix)
{
    if (ix != -1) {
	// If we insert a track in th middle of a project, adjust the parent track for all clips below
    	QPtrListIterator < DocTrackBase > itt(m_tracks);

    	if (ix > m_tracks.count())
		ix = m_tracks.count();
    	else itt += ix;
    	while (itt) {
		(*itt)->trackIndexChanged(trackIndex(itt) + 1);
		itt += 1;
    	}
    }

    if (ix == -1) ix = m_tracks.count();
    //m_tracks.append(track);
    m_tracks.insert(ix, track);
    track->trackIndexChanged(ix); //trackIndex(track));
    connectTrack(track);
    emit trackListChanged();
}

void DocClipProject::slotDeleteTrack(int ix)
{
    
    // If we delete a track in the middle of a project, adjust the parent track for all clips below
    QPtrListIterator < DocTrackBase > itt(m_tracks);

    if (ix >= m_tracks.count())
	ix = m_tracks.count();
    else itt += ix + 1;
    while (itt) {
	(*itt)->trackIndexChanged(trackIndex(itt) - 1);
	itt += 1;
    }

    m_tracks.remove(ix);
    emit trackListChanged();
}

/** Returns the index value for this track, or -1 on failure.*/
int DocClipProject::trackIndex(DocTrackBase * track)
{
    return m_tracks.find(track);
}

void DocClipProject::connectTrack(DocTrackBase * track)
{
//    connect(track, SIGNAL(()), this,
//	SIGNAL(clipLayoutChanged()));
    connect(track, SIGNAL(signalClipSelected(DocClipRef *)), this,
	SIGNAL(signalClipSelected(DocClipRef *)));
    connect(track, SIGNAL(signalOpenClip(DocClipRef *)), this,
	SIGNAL(signalOpenClip(DocClipRef *)));
    connect(track, SIGNAL(trackLengthChanged(const GenTime &)), this,
	SLOT(slotCheckProjectLength()));
    connect(track, SIGNAL(effectStackChanged(DocClipRef *)), this,
	SIGNAL(effectStackChanged(DocClipRef *)));
}

uint DocClipProject::numTracks() const
{
    return m_tracks.count();
}

DocTrackBase *DocClipProject::findTrack(DocClipRef * clip) const
{
    DocTrackBase *returnTrack = 0;

    QPtrListIterator < DocTrackBase > itt(m_tracks);

    for (DocTrackBase * track; (track = itt.current()); ++itt) {
	if (track->clipExists(clip)) {
	    returnTrack = track;
	    break;
	}
    }

    return returnTrack;
}

DocTrackBase *DocClipProject::track(uint track)
{
    return m_tracks.at(track);
}

const DocTrackBase & DocClipProject::track(uint track) const
{
    QPtrListIterator < DocTrackBase > itt(m_tracks);

    if (track >= m_tracks.count())
	track = m_tracks.count() - 1;
    itt += track;

    return *(itt.current());
}

bool DocClipProject::moveSelectedClips(GenTime startOffset,
    int trackOffset)
{
    // For each track, check and make sure that the clips can be moved to their rightful place. If
    // one cannot be moved to a particular location, then none of them can be movRef        // We check for the closest position the track could possibly be moved to, and move it there instead.

    int destTrackNum;
    DocTrackBase *srcTrack, *destTrack;
    GenTime clipStartTime;
    GenTime clipEndTime;
    DocClipRef *srcClip, *destClip;

    blockTrackSignals(true);

    for (uint track = 0; track < numTracks(); track++) {
	srcTrack = m_tracks.at(track);
	if (srcTrack->hasSelectedClips() == 0)
	    continue;

	destTrackNum = track + trackOffset;

  if ((destTrackNum < 0) || (destTrackNum >= (int) numTracks()))
	    return false;	// This track will be moving it's clips out of the timeline, so fail automatically.

	destTrack = m_tracks.at(destTrackNum);

	QPtrListIterator < DocClipRef > srcClipItt =
	    srcTrack->firstClip(true);
	QPtrListIterator < DocClipRef > destClipItt =
	    destTrack->firstClip(false);

	destClip = destClipItt.current();

	while ((srcClip = srcClipItt.current()) != 0) {
	    clipStartTime =
		srcClipItt.current()->trackStart() + startOffset;
	    clipEndTime =
		clipStartTime + srcClipItt.current()->cropDuration();

	    // Make video clips can only be dropped on video tracks and audio on 
	    // audio tracks.
/*			if ((srcClip->clipType() == VIDEO || srcClip->clipType() == AV) && destTrack->clipType() != "Video") return false;
			else if (srcClip->clipType() == AUDIO && destTrack->clipType() != "Sound") return false;*/

	    while ((destClip)
		&& (destClip->trackStart() + destClip->cropDuration() <=
		    clipStartTime)) {
		++destClipItt;
		destClip = destClipItt.current();
	    }
	    if (destClip == 0)
		break;

	    if (destClip->trackStart() < clipEndTime) {
		blockTrackSignals(false);
		return false;
	    }

	    ++srcClipItt;
	}
    }

    // we can now move all clips where they need to be.

    // If the offset is negative, handle tracks from forwards, else handle tracks backwards. We
    // do this so that there are no collisions between selected clips, which would be caught by DocTrackBase
    // itself.

    int startAtTrack, endAtTrack, direction;

    if (trackOffset < 0) {
	startAtTrack = 0;
	endAtTrack = numTracks();
	direction = 1;
    } else {
	startAtTrack = numTracks() - 1;
	endAtTrack = -1;
	direction = -1;
    }

    for (int track = startAtTrack; track != endAtTrack; track += direction) {
	srcTrack = m_tracks.at(track);
	if (srcTrack->hasSelectedClips() == 0)
	    continue;
	srcTrack->moveClips(startOffset, true);

	if (trackOffset) {
	    destTrackNum = track + trackOffset;
	    destTrack = m_tracks.at(destTrackNum);
	    destTrack->addClips(srcTrack->removeClips(true), true);
	}
    }

    blockTrackSignals(false);

    slotCheckProjectLength();

    return true;
}

void DocClipProject::blockTrackSignals(bool block)
{
    QPtrListIterator < DocTrackBase > itt(m_tracks);

    while (itt.current() != 0) {
	itt.current()->blockSignals(block);
	++itt;
    }
}

void DocClipProject::fixClipDuration(KURL url, GenTime length)
{
    QPtrListIterator < DocTrackBase > trackItt(m_tracks);
    while (trackItt.current()) {
	DocTrackClipIterator itt(*(trackItt.current()));
	while (itt.current()) {
	    if (itt.current()->fileURL() == url
		&& itt.current()->cropDuration() > length)
		itt.current()->setCropDuration(length);
	    ++itt;
	}
	++trackItt;
    }
    emit trackListChanged();
}

int DocClipProject::playlistTrackNum(int ix) const
{
    int result = 0;
    int audioTracks = 0;
    bool isAudioTrack = false;
    QPtrListIterator < DocTrackBase > trackItt(m_tracks);
    while (ix>0) {
	if (trackItt.current()->clipType() != "Sound") audioTracks ++; 
        ++trackItt;
	if (!trackItt) return 0; 
        ix--;
    }
    if (trackItt.current()->clipType() == "Sound") isAudioTrack = true;
    while (trackItt) {
        if  (trackItt.current()->clipType() != "Sound") result++;
	else if (isAudioTrack) audioTracks ++; ;
        ++trackItt;
    }
    if (isAudioTrack) result += audioTracks;
    return result;
}

int DocClipProject::playlistNextVideoTrack(int ix) const
{
    int result = 0;
    QPtrListIterator < DocTrackBase > trackItt(m_tracks);
    while (ix>-1) {
        ++trackItt;
	if (!trackItt) return 0;
        ix--;
    }
    while (trackItt) {
        if  (trackItt.current()->clipType() != "Sound") result++;
        ++trackItt;
    }
    return result;
}

QDomDocument DocClipProject::generateSceneList(bool addProducers, bool rendering) const
{
    //kdDebug()<<"+++++++++++  Generating scenelist start...  ++++++++++++++++++"<<endl;
    QDomDocument doc;
    QStringList videoTracks;
    int tracknb = 0;
    uint tracksCounter = 0;
    if (duration().frames(framesPerSecond()) == 0) return QDomDocument();

    QString projectLastFrame = QString::number(duration().frames(framesPerSecond()) - 1);

    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);

    QDomDocumentFragment clipTransitions = doc.createDocumentFragment();
    
    /* import the list of all producer clips */
    if (addProducers) {
	westley.appendChild(doc.importNode(producersList, true));
	westley.appendChild(doc.importNode(virtualProducersList, true));
    }

    QDomElement tractor = doc.createElement("tractor");
    QDomElement multitrack = doc.createElement("multitrack");

    QDomDocumentFragment audiotrack = doc.createDocumentFragment();

    QPtrListIterator < DocTrackBase > trackItt(m_tracks);
    QPtrListIterator < DocTrackBase > trackCounter(m_tracks);
    
    // Add black clip as first track, so that empty spaces appear black 
    // (looks like color producer cannot be longer than 15000 frames, so hack around it... 
    QDomElement playlist = doc.createElement("playlist");
    int dur = duration().frames(framesPerSecond()) - 1;
    while (dur > 14000) {
        QDomElement blank = doc.createElement("entry");
        blank.setAttribute("in", "0");
        blank.setAttribute("out", QString::number(13999));
        blank.setAttribute("producer", "black");
        playlist.appendChild(blank);
	dur = dur - 14000;
    }
	 
    QDomElement blank = doc.createElement("entry");
    blank.setAttribute("in", "0");
    blank.setAttribute("out", QString::number(dur));
    blank.setAttribute("producer", "black");
    playlist.appendChild(blank);
    multitrack.appendChild(playlist);

    // parse the tracks in reverse order so that the upper tracks appear in front of the lower ones
    trackItt.toLast();
    while (trackItt.current()) {
	bool isBlind = trackItt.current()->isBlind();
	bool isMute = trackItt.current()->isMute();
	DocTrackClipIterator itt(*(trackItt.current()));
	QDomElement playlist = doc.createElement("playlist");
	playlist.setAttribute("id",
	    QString("playlist") + QString::number(tracknb++));
	if (trackItt.current()->clipType() == "Sound") playlist.setAttribute("hide", "video");
        else if (isBlind) playlist.setAttribute("hide", "video");

	int timestart = 0;
        bool hideTrack = false;
        if (trackItt.current()->isMute()) 
        {
            if (isBlind || trackItt.current()->clipType() == "Sound") hideTrack = true;
            else playlist.setAttribute("hide", "audio");
        }
        
        if (!hideTrack)
        while (itt.current()) {
	    if (itt.current()->trackStart().frames(framesPerSecond()) -
		timestart > 0.01) {
		QDomElement blank = doc.createElement("blank");
		blank.setAttribute("length",
		    QString::number(itt.current()->trackStart().
			frames(framesPerSecond()) - timestart));
		playlist.appendChild(blank);
	    }

            // Insert xml describing clip
            playlist.appendChild(itt.current()->generateXMLClip(rendering).firstChild());

            // Append clip's transitions for video tracks
            if ((KdenliveSettings::showtransitions() && !KdenliveSettings::multitrackview()) || rendering) clipTransitions.insertBefore(doc.importNode(itt.current()->generateXMLTransition(isBlind, isMute), true), QDomNode());

	    timestart = (int)itt.current()->trackEnd().frames(framesPerSecond());
	    ++itt;
	}
        if (trackItt.current()->clipType() == "Sound") audiotrack.appendChild(playlist);
        else {
	    multitrack.appendChild(playlist);	
	    videoTracks.append(QString::number(tracksCounter));
	}
	tracksCounter++;
	--trackItt;
    }
    // add audio tracks to the multitrack
    multitrack.appendChild(audiotrack);
    tractor.appendChild(multitrack);
    
        // Add all transitions
        
    if (KdenliveSettings::showtransitions() || rendering) tractor.appendChild(clipTransitions);
        
    /* transition: mix all used audio tracks */
    
    if (tracksCounter > 1) {
	uint firstNonMutedTrack = 1;
	QDomNode currTrack = multitrack.firstChild();
	// skip first mute black track
	currTrack = currTrack.nextSibling();
	while (currTrack != QDomNode() && currTrack.toElement().attribute("hide") == "audio") {
	    currTrack = currTrack.nextSibling();
	    firstNonMutedTrack++;
	}
	currTrack = currTrack.nextSibling();
        for (uint i = firstNonMutedTrack + 1; i <tracksCounter +1 ; i++) {
	    // Is the track muted
	    if (currTrack.toElement().attribute("hide") != "audio") {
		//kdDebug()<<"+++   MIXING TRACK "<<i<<", "<<currTrack.toElement().attribute("id")<<endl;
	        QDomElement transition = doc.createElement("transition");
	        transition.setAttribute("in", "0");
                transition.setAttribute("out", projectLastFrame);
                transition.setAttribute("a_track", QString::number(firstNonMutedTrack));
	        transition.setAttribute("b_track", QString::number(i));
	        transition.setAttribute("mlt_service", "mix");
                transition.setAttribute("combine", "1");
	        tractor.appendChild(transition);
	    }
	    //else kdDebug()<<"+++   TRACK "<<i<<" is MUTED : "<<currTrack.toElement().attribute("id")<<endl;
	    currTrack = currTrack.nextSibling();
	}
    }

    if (KdenliveSettings::multitrackview() && !rendering)
    for (uint i = 0; i <4 && i < videoTracks.count(); i++) {
	    QDomElement transition = doc.createElement("transition");
	    transition.setAttribute("in", "0");
            transition.setAttribute("out", projectLastFrame);
	    QString geom;
	    switch (i) {
	        case 0:
		    geom = "0,0:50%x50%";
		    break;
	        case 1:
		    geom = "50%,0:50%x50%";
		    break;
	        case 2:
		    geom = "0,50%:50%x50%";
		    break;
	        case 3:
		    geom = "50%,50%:50%x50%";
		    break;
	    }
	    transition.setAttribute("geometry", geom);
	    transition.setAttribute("distort", "1");
            transition.setAttribute("a_track", "0");
	    transition.setAttribute("b_track", QString::number(playlistTrackNum(tracksCounter - 1 - videoTracks[i].toInt())));
	    transition.setAttribute("mlt_service", "composite");
	    tractor.appendChild(transition);
    }

    westley.appendChild(tractor);
         //kdDebug() << doc.toString() << endl;
         //kdDebug()<<"+++++++++++  Generating scenelist end...  ++++++++++++++++++"<<endl;
    return doc;
}



QDomDocument DocClipProject::generatePartialSceneList(GenTime start, GenTime end, int prodId) const
{
    QValueList <int> usedProducersList;
    QDomDocument doc;
    int tracknb = 0;
    uint tracksCounter = 0;
    if (duration().frames(framesPerSecond()) == 0) return QDomDocument();

    QString projectLastFrame = QString::number((end - start).frames(framesPerSecond()) - 1);

    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);

    QDomDocumentFragment clipTransitions = doc.createDocumentFragment();
    QDomElement blackprod = doc.createElement("producer");
    blackprod.setAttribute("id", "black");
    blackprod.setAttribute("mlt_service", "colour");
    blackprod.setAttribute("colour", "black");
    westley.appendChild(blackprod);

    QDomElement tractor = doc.createElement("tractor");
    QDomElement multitrack = doc.createElement("multitrack");

    QDomDocumentFragment audiotrack = doc.createDocumentFragment();

    QPtrListIterator < DocTrackBase > trackItt(m_tracks);
    QPtrListIterator < DocTrackBase > trackCounter(m_tracks);
    
    // Add black clip as first track, so that empty spaces appear black 
    // (looks like color producer cannot be longer than 15000 frames, so hack around it... 
    QDomElement playlist = doc.createElement("playlist");
    int dur = (end - start).frames(framesPerSecond()) - 1;

    // black background	 track
    while (dur > 14000) {
        QDomElement blank = doc.createElement("entry");
        blank.setAttribute("in", "0");
        blank.setAttribute("out", QString::number(13999));
        blank.setAttribute("producer", "black");
        playlist.appendChild(blank);
	dur = dur - 14000;
    }
    QDomElement blank = doc.createElement("entry");
    blank.setAttribute("in", "0");
    blank.setAttribute("out", QString::number(dur));
    blank.setAttribute("producer", "black");
    playlist.appendChild(blank);
    multitrack.appendChild(playlist);

    // parse the tracks in reverse order so that the upper tracks appear in front of the lower ones
    trackItt.toLast();
    while (trackItt.current()) {
	bool isBlind = trackItt.current()->isBlind();
	bool isMute = trackItt.current()->isMute();
	DocTrackClipIterator itt(*(trackItt.current()));
	QDomElement playlist = doc.createElement("playlist");
	playlist.setAttribute("id",
	    QString("playlist") + QString::number(tracknb++));
	if (trackItt.current()->clipType() == "Sound") playlist.setAttribute("hide", "video");
        else if (isBlind) playlist.setAttribute("hide", "video");

	int timestart = start.frames(framesPerSecond());
        bool hideTrack = false;
        if (trackItt.current()->isMute()) 
        {
            if (isBlind || trackItt.current()->clipType() == "Sound") hideTrack = true;
            else playlist.setAttribute("hide", "audio");
        }
        
        if (!hideTrack)
        while (itt.current()) {
 	    if (itt.current()->trackStart() < end && itt.current()->trackEnd() > start) {
		if (usedProducersList.findIndex(itt.current()->referencedClip()->getId()) == -1)
		    usedProducersList.append(itt.current()->referencedClip()->getId());
	        if (itt.current()->trackStart().frames(framesPerSecond()) - timestart > 0.01) {
		    QDomElement blank = doc.createElement("blank");
		    blank.setAttribute("length", QString::number(itt.current()->trackStart().frames(framesPerSecond()) - timestart));
		    playlist.appendChild(blank);
	        }
                // Insert xml describing clip
                playlist.appendChild(itt.current()->generateOffsetXMLClip(start, end).firstChild());

                // Append clip's transitions for video tracks
                clipTransitions.insertBefore(doc.importNode(itt.current()->generateOffsetXMLTransition(isBlind, isMute, start, end), true), QDomNode());

	        timestart = (int)(itt.current()->trackEnd()).frames(framesPerSecond());
	    }
	    ++itt;
	
	}
	    tracksCounter++;
            if (trackItt.current()->clipType() == "Sound") audiotrack.appendChild(playlist);
            else multitrack.appendChild(playlist);
	--trackItt;
    }
    // add audio tracks to the multitrack
    multitrack.appendChild(audiotrack);
    tractor.appendChild(multitrack);
    
        // Add all transitions
        
    tractor.appendChild(clipTransitions);
        
    /* transition: mix all used audio tracks */
    
    if (tracksCounter > 1)
        for (uint i = 2; i <tracksCounter +1 ; i++) {
	    QDomElement transition = doc.createElement("transition");
	    transition.setAttribute("in", "0");
            transition.setAttribute("out", projectLastFrame);
            transition.setAttribute("a_track", QString::number(1));
	    transition.setAttribute("b_track", QString::number(i));
	    transition.setAttribute("mlt_service", "mix");
            transition.setAttribute("combine", "1");
	    tractor.appendChild(transition);
	}


    // Add used producers only
    QDomDocumentFragment nprods = producersList;
    QDomNode node = nprods.firstChild();
	while (!node.isNull()) {
	    QDomElement e = node.toElement();
	    if (!e.isNull()) {
		int ix = e.attribute("id", "-1").toInt();
		if (usedProducersList.findIndex(ix) != -1)
		    westley.appendChild(doc.importNode(e, true));
	    }
	    node = node.nextSibling();
	}

    QDomDocumentFragment vprods = virtualProducersList;
    node = vprods.firstChild();
	while (!node.isNull()) {
	    QDomElement e = node.toElement();
	    if (!e.isNull()) {
		int ix = e.attribute("id", "-1").toInt();
		// Don't allow embedding of producer in itself
		if (ix != prodId && usedProducersList.findIndex(ix) != -1)
		    westley.appendChild(doc.importNode(e, true));
	    }
	    node = node.nextSibling();
	}


    westley.appendChild(tractor);
        // kdDebug() << doc.toString() << endl;
        // kdDebug()<<"+++++++++++  Generating PARTIAL scenelist end...  ++++++++++++++++++"<<endl;
    return doc;
}


void DocClipProject::
generateTracksFromXML(KdenliveDoc *doc, const QDomElement & e)
{
    m_tracks.generateFromXML(doc, this, e);
    DocTrackBaseListIterator itt(m_tracks);
    while (itt.current() != 0) {
	connectTrack(itt.current());
	itt.current()->checkTrackLength();
	++itt;
    }
    slotCheckProjectLength();
}

//static
DocClipProject *DocClipProject::
createClip(KdenliveDoc *doc, const QDomElement element)
{
    DocClipProject *project = new DocClipProject(KdenliveSettings::defaultfps(), KdenliveSettings::defaultwidth(), KdenliveSettings::defaultheight());
    project->generateTracksFromXML(doc, element);
    
/*
    if (element.tagName() == "project") {
	KURL url(element.attribute("url"));
	double framesPerSecond = element.attribute("fps", QString::number(KdenliveSettings::defaultfps())).toDouble();
        int videoWidth = element.attribute("videowidth", QString::number(KdenliveSettings::defaultwidth())).toInt();
        int videoHeight = element.attribute("videoheight", QString::number(KdenliveSettings::defaultheight())).toInt();
        project = new DocClipProject(framesPerSecond, videoWidth, videoHeight);

	QDomNode node = element.firstChild();

	while (!node.isNull()) {
	    QDomElement e = node.toElement();
	    if (!e.isNull()) {
		if (e.tagName() == "DocTrackBaseList") {
		    project->generateTracksFromXML(effectList, clipManager,
			e);
		}
	    }
	    node = node.nextSibling();
	}
    } else {
	kdWarning() << "DocClipProject::createClip failed to generate clip"
	    << endl;
    }*/

    return project;
}

const DocClipBase::CLIPTYPE & DocClipProject::clipType() const
{
    return NONE;
}

void DocClipProject::populateSceneTimes(QValueVector < GenTime > &toPopulate) const
{
    GenTime time;

    QPtrListIterator < DocTrackBase > trackItt(m_tracks);

    while (trackItt.current()) {
	DocTrackClipIterator itt(*(trackItt.current()));

	while (itt.current()) {
	    QValueVector < GenTime > newTimes;
	    itt.current()->populateSceneTimes(newTimes);
	    QValueVector < GenTime >::Iterator newItt = newTimes.begin();

	    while (newItt != newTimes.end()) {
		time = (*newItt);
		if ((time >= GenTime(0)) && (time <= duration())) {
		    toPopulate.append(time);
		}
		++newItt;

	    }
	    ++itt;
	}

	++trackItt;
    }

    toPopulate.append(GenTime(0));
    toPopulate.append(duration());
}

// Returns an XML document that describes part of the current scene.
//virtual
QDomDocument DocClipProject::sceneToXML(const GenTime & startTime, const GenTime & endTime) const
{
    QDomDocument doc;

    QPtrListIterator < DocTrackBase > trackItt(m_tracks);

    // For the moment, this only returns the most relevant clip on the top most track.
    while (trackItt.current()) {
	DocTrackClipIterator itt(*(trackItt.current()));

	while (itt.current()) {
	    DocClipRef *clip = itt.current();

	    if ((clip->trackStart() <= startTime)
		&& (clip->trackEnd() >= endTime)) {
		doc = clip->sceneToXML(startTime, endTime);
		break;
	    }

	    ++itt;
	}
	++trackItt;
    }

    return doc;
}

DocClipRef *DocClipProject::getClipAt(int track, GenTime time)
{
    return m_tracks.at(track)->getClipAt(time);
}

int DocClipProject::hasSelectedClips()
{
    int result = 0;

    for (uint count = 0; count < numTracks(); ++count) {
	result += m_tracks.at(count)->hasSelectedClips();
    }
    return result;
}

DocClipRef *DocClipProject::selectedClip()
{
    DocClipRef *pResult = 0;
    DocTrackBase *srcTrack = 0;

    for (uint track = 0; track < numTracks(); track++) {
	srcTrack = m_tracks.at(track);
	if (srcTrack->hasSelectedClips() > 0) {
	    pResult = srcTrack->firstClip(true).current();
            break;
	}
    }

    return pResult;
}

// virtual
bool DocClipProject::referencesClip(DocClipBase * clip) const
{
    bool result = false;

    DocTrackBase *srcTrack;
    uint track = 0;
    QPtrListIterator < DocTrackBase > itt(m_tracks);

    while (itt.current()) {
	srcTrack = itt.current();

	if (srcTrack->referencesClip(clip)) {
	    result = true;
	    break;
	}

	++itt;
	++track;
    }

    return result;
}

DocClipRefList DocClipProject::selectedClipList()
{
    DocClipRefList list;
    QPtrListIterator < DocTrackBase > trackItt(m_tracks);
    while (trackItt.current()) {
	list.appendList(trackItt.current()->selectedClips());
            ++trackItt;
        }
    return list;
}

DocClipRefList DocClipProject::referencedClips(DocClipBase * clip)
{
    DocClipRefList list;
    DocTrackBase *srcTrack;
    for (uint track = 0; track < numTracks(); ++track) {
	srcTrack = m_tracks.at(track);
	list.appendList(srcTrack->referencedClips(clip));
    }

    return list;
}

// virtual
QDomDocument DocClipProject::toXML()
{
    QDomDocument doc = DocClipBase::toXML();
    QDomNode node = doc.firstChild();

    while (!node.isNull()) {
	QDomElement element = node.toElement();
	if (!element.isNull()) {
	    if (element.tagName() == "kdenliveclip") {
		element.appendChild(doc.importNode(m_tracks.toXML().
			documentElement(), true));
		return doc;
	    }
	}
	node = node.nextSibling();
    }

    assert(node.isNull());

    return doc;
}

// virtual
bool DocClipProject::matchesXML(const QDomElement & element) const
{
    bool result = false;

    if (element.tagName() == "kdenliveclip") {
	QDomNodeList nodeList = element.elementsByTagName("project");
	if (nodeList.length() > 0) {
	    if (nodeList.length() > 1) {
		kdWarning() <<
		    "More than one element named project in XML,only matching XML to first one!"
		    << endl;
	    }

	    QDomElement clip = nodeList.item(0).toElement();
	    if (!clip.isNull()) {
# warning "this line might be the cause of significant trouble"
		if (element.attribute("fps").toDouble() ==
		    m_framesPerSecond) {
		    QDomNodeList trackNodeList =
			clip.elementsByTagName("DocTrackBaseList");
		    if (trackNodeList.length() > 0) {
			if (trackNodeList.length() > 1) {
			    kdWarning() <<
				"More than one element named 'DocTrackBaseList' in XML, matching only the first one!"
				<< endl;
			}
			QDomElement trackElement =
			    trackNodeList.item(0).toElement();
			if (!trackElement.isNull()) {
			    result = m_tracks.matchesXML(trackElement);
			}
		    }
		}
	    }
	}
    }

    return result;
}


bool DocClipProject::canAddClipsToTracks(DocClipRefList & clips, int track,
    const GenTime & clipOffset) const
{
    QPtrListIterator < DocClipRef > itt(clips);
    int trackOffset;
    GenTime startOffset;

    if (clips.masterClip()) {
	trackOffset = clips.masterClip()->trackNum();
	startOffset = clipOffset - clips.masterClip()->trackStart();
    } else {
	trackOffset = clips.first()->trackNum();
	startOffset = clipOffset - clips.first()->trackStart();
    }
    if (trackOffset == -1)
	trackOffset = 0;
    trackOffset = track - trackOffset;

    while (itt.current()) {
	itt.current()->moveTrackStart(itt.current()->trackStart() +
	    startOffset);
	++itt;
    }
    itt.toFirst();

    while (itt.current()) {
	if (!itt.current()->durationKnown()) {
	    kdWarning() << "Clip Duration not known, cannot add clips" <<
		endl;
	    return false;
	}

	int curTrack = itt.current()->trackNum();
	if (curTrack == -1)
	    curTrack = 0;
	curTrack += trackOffset;

  if ((curTrack < 0) || (curTrack >= (int) numTracks())) {
	    return false;
	}

	if (!DocClipProject::track(curTrack).canAddClip(itt.current())) {
	    return false;
	}

	++itt;
    }

    return true;
}

void DocClipProject::slotCheckProjectLength()
{
    GenTime length(0);

    QPtrListIterator < DocTrackBase > itt(m_tracks);

    while (itt.current()) {
	GenTime test = itt.current()->trackLength();
	length = (test > length) ? test : length;
	++itt;
    }

    if (m_projectLength != length) {
	m_projectLength = length;
	emit projectLengthChanged(m_projectLength);
    }
}

bool DocClipProject::clipSelected(DocClipRef * clip)
{
    bool result = false;

    DocTrackBase *track = findTrack(clip);

    if (track) {
	result = track->clipSelected(clip);
    } else {
	kdWarning() <<
	    "DocClipProject::clipSelected() - clip does not exist in project!"
	    << endl;
    }

    return result;
}

const DocTrackBaseList & DocClipProject::trackList() const
{
    return m_tracks;
}
