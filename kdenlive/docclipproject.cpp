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

#include <kdebug.h>

#include "doctrackbase.h"
#include "doctrackclipiterator.h"

DocClipProject::DocClipProject(double framesPerSecond):DocClipBase(),
m_framesPerSecond(framesPerSecond)
{
    producersList = QDomDocument();
    m_tracks.setAutoDelete(true);
}

DocClipProject::~DocClipProject()
{
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

/** Adds a track to the project */
void DocClipProject::addTrack(DocTrackBase * track)
{
    m_tracks.append(track);
    track->trackIndexChanged(trackIndex(track));
    connectTrack(track);
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
    connect(track, SIGNAL(clipChanged(DocClipRef *)), this,
	SIGNAL(clipChanged(DocClipRef *)));
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
	if (!srcTrack->hasSelectedClips())
	    continue;

	destTrackNum = track + trackOffset;

	if ((destTrackNum < 0) || (destTrackNum >= numTracks()))
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
	if (!srcTrack->hasSelectedClips())
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

QDomDocument DocClipProject::generateSceneList() const
{
    QDomDocument doc;
    int tracknb = 0;
    uint tracksCounter = 0;

    QString projectDuration = QString::number(duration().frames(framesPerSecond()));

    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);


    /* import the list of all producer clips */
    westley.appendChild(producersList);
    QDomElement tractor = doc.createElement("tractor");
    QDomElement multitrack = doc.createElement("multitrack");
    

    QPtrListIterator < DocTrackBase > trackItt(m_tracks);
    
    // Add black clip as first track, so that empty spaces appear black
    QDomElement playlist = doc.createElement("playlist");
    QDomElement blank = doc.createElement("entry");
    blank.setAttribute("in", "0");
    blank.setAttribute("out", projectDuration);
    blank.setAttribute("producer", "black");
    playlist.appendChild(blank);
    multitrack.appendChild(playlist);

    while (trackItt.current()) {
	DocTrackClipIterator itt(*(trackItt.current()));
	QDomElement playlist = doc.createElement("playlist");
	playlist.setAttribute("id",
	    QString("playlist") + QString::number(tracknb++));
	if (trackItt.current()->clipType() == "Sound")
	    playlist.setAttribute("hide", "video");
	//if (trackItt.current()->clipType() == "Video") playlist.setAttribute("hide", "audio");

	int children = 0;
	int timestart = 0;
	while (itt.current()) {
	    if (itt.current()->trackStart().frames(framesPerSecond()) -
		timestart > 0.01) {
		QDomElement blank = doc.createElement("blank");
		blank.setAttribute("length",
		    QString::number(itt.current()->trackStart().
			frames(framesPerSecond()) - timestart));
		playlist.appendChild(blank);
	    }

            playlist.appendChild(itt.current()->generateXMLClip().firstChild());

	    timestart =
                    itt.current()->trackEnd().frames(framesPerSecond());
	    children++;
	    ++itt;
	}
	    tracksCounter++;
	    multitrack.appendChild(playlist);
	++trackItt;
    }
    
    tractor.appendChild(multitrack);
    
    
    /* transition: mix all used audio tracks */
    
    if (tracksCounter > 1)
	for (int i = 1; i < tracksCounter; i++) {
	    QDomElement transition = doc.createElement("transition");
	    transition.setAttribute("in", "0");
            transition.setAttribute("out", projectDuration);
	    /*transition.setAttribute("a_track", QString::number(usedAudioTracks[i]));
	       transition.setAttribute("b_track", QString::number(usedAudioTracks[i+1])); */
	    transition.setAttribute("a_track", QString::number(i));
	    transition.setAttribute("b_track", QString::number(i + 1));
	    transition.setAttribute("mlt_service", "mix");
	    transition.setAttribute("start", "0.5");
	    transition.setAttribute("end", "0.5");
	    tractor.appendChild(transition);
	}
        
        /* Add transitions, currently only crossfade luma supported */
        TransitionStack::iterator itt = m_transitionStack.begin();
        uint index = m_transitionStack.count();
            for (uint count = 0; count < index; ++count)
            {
                QDomElement transition = doc.createElement("transition");
                transition.setAttribute("in", QString::number((*itt)->transitionStartTime()));
                transition.setAttribute("out", QString::number((*itt)->transitionEndTime()));
                transition.setAttribute("a_track", QString::number((*itt)->transitionStartTrack()+1));
                transition.setAttribute("b_track", QString::number((*itt)->transitionEndTrack()+1));
                transition.setAttribute("mlt_service", "luma");
                transition.setAttribute("reverse", "0");
                tractor.appendChild(transition);   
                ++itt;
            }

    doc.documentElement().appendChild(tractor);

    kdDebug() << "+ + + PROJECT SCENE: " << doc.toString() << endl;
    return doc;
}


TransitionStack DocClipProject::clipHasTransition(DocClipRef *clip)
{
    TransitionStack result;
    TransitionStack::iterator itt = m_transitionStack.begin();
    uint count = 0;
    while (itt)
    {
        Transition *tra = (*itt)->hasClip(clip);
        if (tra) {
            count++;
            result.append(tra->clone());
        }
        ++itt;
    }
    return result;
}
/*
QDomDocument DocClipProject::generateSceneList() const
{
kdDebug()<<"GENERATING PROJECT CLIP SCENE"<<endl;
QDomDocument doc;
	QDomElement elem=doc.createElement("westley");
	doc.appendChild(elem);
	QDomElement elem1=doc.createElement("producer");
	elem1.setAttribute("id","resource0");
	elem.appendChild(elem1);
	QDomElement elem2=doc.createElement("property");
	elem2.setAttribute("name","resource");
	elem1.appendChild(elem2);
	QDomText elem3=doc.createTextNode("/home/one/Desktop/dvgrab-001.dv");
	elem2.appendChild(elem3);



	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";

	QDomDocument sceneList;
	sceneList.appendChild(sceneList.createElement("scenelist"));

	QValueVector<GenTime> unsortedTimes;
        populateSceneTimes(unsortedTimes);

	// sort times and remove duplicates.
	qHeapSort(unsortedTimes);
	QValueVector<GenTime> times;

	// Will reserve much more than we need, but should speed up the process somewhat...
	times.reserve(unsortedTimes.size());

	QValueVector<GenTime>::Iterator itt = unsortedTimes.begin();
	while(itt != unsortedTimes.end()) {
		if((times.isEmpty()) || ((*itt) != times.last())) {
			times.append(*itt);
		}
		++itt;
	}

	QValueVector<GenTime>::Iterator sceneItt = times.begin();

	while( (sceneItt != times.end()) && (sceneItt+1 != times.end()) ) {
		QDomElement scene = sceneList.createElement("par");

		QDomDocument clipDoc = sceneToXML(*sceneItt, *(sceneItt+1));
		QDomElement sceneClip;

		if(clipDoc.documentElement().isNull()) {
			sceneClip = sceneList.createElement("img");
			sceneClip.setAttribute("rgbcolor", "#000000");
			sceneClip.setAttribute("dur", QString::number((*(sceneItt+1) - *sceneItt).seconds(), 'f', 10));
			scene.appendChild(sceneClip);
		} else {
			sceneClip = sceneList.importNode(clipDoc.documentElement(), true).toElement();
			scene.appendChild(sceneClip);
		}

		sceneList.documentElement().appendChild(scene);

		++sceneItt;
	}

	return sceneList;
}
*/

void DocClipProject::
generateTracksFromXML(const EffectDescriptionList & effectList,
    ClipManager & clipManager, const QDomElement & e)
{
    m_tracks.generateFromXML(effectList, clipManager, this, e);

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
createClip(const EffectDescriptionList & effectList,
    ClipManager & clipManager, const QDomElement element)
{
    DocClipProject *project = 0;

    if (element.tagName() == "project") {
	KURL url(element.attribute("url"));
	double framesPerSecond = element.attribute("fps", "25").toDouble();
	project = new DocClipProject(framesPerSecond);

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
    }

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

bool DocClipProject::hasSelectedClips()
{
    bool result = false;

    for (uint count = 0; count < numTracks(); ++count) {
	if (m_tracks.at(count)->hasSelectedClips()) {
	    result = true;
	    break;
	}
    }

    return result;
}

bool DocClipProject::hasTwoSelectedClips()
{
    // #FIXME: Currently, counts the number of tracks that have selected clips.
    // It should count the total number of selected clips...
    uint nb = 0;

    for (uint count = 0; count < numTracks(); ++count) {
        if (m_tracks.at(count)->hasSelectedClips())
            nb++;
    }

    if (nb == 2) return true;
    return false;
}


void DocClipProject::deleteTransition()
{
    /* Currently only deletes transitions for the first selected clip */
    DocClipRef *selectedClip = 0;
    DocTrackBase *srcTrack = 0;

    for (uint track = 0; track < numTracks(); track++) {
        srcTrack = m_tracks.at(track);
        if (srcTrack->hasSelectedClips()) {
            selectedClip = srcTrack->firstClip(true).current();
            break;
        }
    }
    if (selectedClip) deleteClipTransition(selectedClip);
}


void DocClipProject::deleteClipTransition(DocClipRef *clip)
{
    /* Currently only deletes transitions for the first selected clip */
    TransitionStack::iterator itt = m_transitionStack.begin();
    while (itt) {
        if ((*itt)->hasClip(clip)) m_transitionStack.remove(*itt);
        else ++itt;
    }
    generateSceneList();
    emit clipLayoutChanged();
}

void DocClipProject::addTransition()
{
    DocClipRef *aResult = 0;
    DocClipRef *bResult = 0;
    DocTrackBase *srcTrack = 0;
    uint ix = 0;

    for (uint track = 0; track < numTracks(); track++) {
        srcTrack = m_tracks.at(track);
        ix++;
        if (srcTrack->hasSelectedClips()) {
            aResult = srcTrack->firstClip(true).current();
            break;
        }
    }
    
    for (uint track = ix; track < numTracks(); track++) {
        srcTrack = m_tracks.at(track);
        if (srcTrack->hasSelectedClips()) {
            bResult = srcTrack->firstClip(true).current();
            break;
        }
    }
    if (!aResult || !bResult) return;
    Transition *transit = new Transition(aResult,bResult);
    m_transitionStack.append(transit);

    generateSceneList();
    emit clipLayoutChanged();
    
}


void DocClipProject::switchTransition()
{
    DocClipRef *aResult = 0;
    DocClipRef *bResult = 0;
    DocTrackBase *srcTrack = 0;
    uint ix = 0;

    for (uint track = 0; track < numTracks(); track++) {
        srcTrack = m_tracks.at(track);
        ix++;
        if (srcTrack->hasSelectedClips()) {
            aResult = srcTrack->firstClip(true).current();
            break;
        }
    }
    
    for (uint track = ix; track < numTracks(); track++) {
        srcTrack = m_tracks.at(track);
        if (srcTrack->hasSelectedClips()) {
            bResult = srcTrack->firstClip(true).current();
            break;
        }
    }
    if (!aResult || !bResult) return;
    Transition *transit = m_transitionStack.exists(aResult,bResult);
    if (transit) m_transitionStack.remove(transit);
    else {
        transit = new Transition(aResult,bResult);
        m_transitionStack.append(transit);
    }

    generateSceneList();
    emit clipLayoutChanged();
}

DocClipRef *DocClipProject::selectedClip()
{
    DocClipRef *pResult = 0;
    DocTrackBase *srcTrack = 0;

    for (uint track = 0; track < numTracks(); track++) {
	srcTrack = m_tracks.at(track);
	if (srcTrack->hasSelectedClips()) {
	    pResult = srcTrack->firstClip(true).current();
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
	    if (element.tagName() == "clip") {
		QDomElement project = doc.createElement("project");
		project.setAttribute("fps",
		    QString::number(m_framesPerSecond, 'f', 10));
		element.appendChild(project);
		project.appendChild(doc.importNode(m_tracks.toXML().
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

    if (element.tagName() == "clip") {
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

	if ((curTrack < 0) || (curTrack >= numTracks())) {
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
