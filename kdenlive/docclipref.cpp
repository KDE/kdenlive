/**************************1*************************************************
                          DocClipRef.cpp  -  description
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

#include <qimage.h>
#include <qdir.h>

#include <kdebug.h>

#include "docclipref.h"

#include "clipmanager.h"
#include "docclipbase.h"
#include "docclipavfile.h"
#include "doccliptextfile.h"
#include "docclipproject.h"
#include "doctrackbase.h"
#include "docclipbase.h"
#include "effectdesc.h"
#include "effectkeyframe.h"
#include "effectparameter.h"
#include "effectparamdesc.h"
#include "effectdoublekeyframe.h"
#include "effectcomplexkeyframe.h"
#include "kdenlivedoc.h"
#include "kdenlivesettings.h"
#include <stdlib.h>

#define AUDIO_FRAME_WIDTH 20
DocClipRef::DocClipRef(DocClipBase * clip):
startTimer(0),
endTimer(0),
m_trackStart(0.0),
m_cropStart(0.0),
m_trackEnd(0.0), m_parentTrack(0),  m_trackNum(-1), m_clip(clip),  m_speed(1.0),  m_endspeed(1.0)
{
    if (!clip) {
	kdError() <<
	    "Creating a DocClipRef with no clip - not a very clever thing to do!!!"
	    << endl;
    }

    DocClipBase::CLIPTYPE t = m_clip->clipType();

    // If clip is a video, resizing it should update the thumbnails
    if (t == DocClipBase::VIDEO || t == DocClipBase::AV || t == DocClipBase::VIRTUAL) {
	m_thumbnail = QPixmap();
        m_endthumbnail = QPixmap();
        startTimer = new QTimer( this );
        endTimer = new QTimer( this );
        connect(clip->thumbCreator, SIGNAL(thumbReady(int, QPixmap)),this,SLOT(updateThumbnail(int, QPixmap)));
        connect(this, SIGNAL(getClipThumbnail(KURL, int, int, int)), clip->thumbCreator, SLOT(getImage(KURL, int, int, int)));
        connect( startTimer, SIGNAL(timeout()), this, SLOT(fetchStartThumbnail()));
        connect( endTimer, SIGNAL(timeout()), this, SLOT(fetchEndThumbnail()));
    }
    if (t == DocClipBase::AUDIO || t == DocClipBase::AV) {
	if (t != DocClipBase::AV){
        	m_thumbnail = referencedClip()->thumbnail();
        	m_endthumbnail = m_thumbnail;
	}
	connect(clip->thumbCreator, SIGNAL(audioThumbReady(QMap<int,QMap<int,QByteArray> >)), this, SLOT(updateAudioThumbnail(QMap<int,QMap<int,QByteArray> >)));
    }
}

DocClipRef::~DocClipRef()
{
    
    delete startTimer;
    delete endTimer;
    //disconnectThumbCreator();
}

void DocClipRef::disconnectThumbCreator()
{
    if (m_clip && m_clip->thumbCreator) {
	disconnect(m_clip->thumbCreator, SIGNAL(audioThumbReady(QMap<int,QMap<int,QByteArray> >)), this, SLOT(updateAudioThumbnail(QMap<int,QMap<int,QByteArray> >)));
	disconnect(this, SIGNAL(getClipThumbnail(KURL, int, int, int)), m_clip->thumbCreator, SLOT(getImage(KURL, int, int, int)));
	disconnect(m_clip->thumbCreator, SIGNAL(thumbReady(int, QPixmap)),this,SLOT(updateThumbnail(int, QPixmap)));
    }
}

void DocClipRef::refreshAudioThumbnail()
{
	if (m_clip->clipType() != DocClipBase::AV && m_clip->clipType() != DocClipBase::AUDIO) return;
	if (KdenliveSettings::audiothumbnails()) {
		if (!m_clip->audioThumbCreated) m_clip->toDocClipAVFile()->getAudioThumbs();
	}
	else {
		if (!m_clip->audioThumbCreated) m_clip->toDocClipAVFile()->stopAudioThumbs();
		m_clip->audioThumbCreated = false;
		referencedClip()->audioFrameChache.clear();
		kdDebug()<<"**********  FREED MEM FOR: "<<name()<<", COUNT: "<<referencedClip()->audioFrameChache.count ()<<endl;
	}
}

void DocClipRef::updateAudioThumbnail(QMap<int,QMap<int,QByteArray> >)
{
    m_clip->audioThumbCreated = true;
    if (m_parentTrack) QTimer::singleShot(200, this, SLOT(refreshCurrentTrack()));
}

void DocClipRef::refreshCurrentTrack()
{
    m_parentTrack->notifyClipChanged(this);
}

bool DocClipRef::hasVariableThumbnails()
{
    DocClipBase::CLIPTYPE t = m_clip->clipType();
    if (( t != DocClipBase::VIDEO && t != DocClipBase::AV && t != DocClipBase::VIRTUAL) || !KdenliveSettings::videothumbnails())
        return false;
    return true;
}

void DocClipRef::generateThumbnails()
{
    DocClipBase::CLIPTYPE t = m_clip->clipType();
    if (t == DocClipBase::VIDEO || t == DocClipBase::AV || t == DocClipBase::VIRTUAL) {
        fetchStartThumbnail();
        fetchEndThumbnail();
	return;
    }

    uint height = (uint)KdenliveSettings::videotracksize();
    uint width = (uint) (height * 1.25);

    QPixmap result(width, height);
    result.fill(Qt::black);

    if (m_clip->clipType() == DocClipBase::COLOR) {
        QPixmap p(width - 2, height - 2);
        QString col = m_clip->toDocClipAVFile()->color();
        col = col.replace(0, 2, "#");
        p.fill(QColor(col.left(7)));
	copyBlt(&result, 1, 1, &p, 0, 0, width - 2, height - 2);
        m_endthumbnail = result;
        m_thumbnail = result;
    }
    else if (m_clip->clipType() == DocClipBase::TEXT || m_clip->clipType() == DocClipBase::IMAGE) {
	    QPixmap p(fileURL().path());
            QImage im;
            im = p;
            p = im.smoothScale(width - 2, height - 2);
	    copyBlt(&result, 1, 1, &p, 0, 0, width - 2, height - 2);
            m_endthumbnail = result;
            m_thumbnail = result;
    }
    else if (m_clip->clipType() == DocClipBase::SLIDESHOW) {
	    QString fileType = fileURL().filename().right(3);
	    QStringList more;
    	    QStringList::Iterator it;
    	    QPixmap p1, p2;
    	    QDir dir( fileURL().directory() );
    	    more = dir.entryList( QDir::Files );
 	
    	    for ( it = more.begin() ; it != more.end() ; ++it ) {
        	if ((*it).endsWith("."+fileType, FALSE)) {
		    p1.load(fileURL().directory() + "/" + *it);
		    break;
		}
    	    }

    	    for ( it = more.end() ; it != more.begin() ; --it ) {
    	        if ((*it).endsWith("."+fileType, FALSE)) {
    	    	    p2.load(fileURL().directory() + "/" + *it);
		    break;
		}
    	    }
    	    QImage im;
    	    im = p1;
    	    p1 = im.smoothScale(width - 2, height - 2);
	    copyBlt(&result, 1, 1, &p1, 0, 0, width - 2, height - 2);
    	    m_thumbnail = result;
	    result.fill(Qt::black);
    	    im = p2;
    	    p2 = im.smoothScale(width - 2, height - 2);
	    copyBlt(&result, 1, 1, &p2, 0, 0, width - 2, height - 2);
    	    m_endthumbnail = result;
    	}
}

const GenTime & DocClipRef::trackStart() const
{
    return m_trackStart;
}


void DocClipRef::setTrackStart(const GenTime time)
{
    m_trackStart = time;

    if (m_parentTrack) {
	m_parentTrack->clipMoved(this);
    }
}

const QString & DocClipRef::name() const
{
    return m_clip->name();
}

const QString & DocClipRef::description() const
{
    return m_clip->description();
}

void DocClipRef::setDescription(const QString & description)
{
    m_clip->setDescription(description);
}

void DocClipRef::fetchStartThumbnail()
{
	uint height = (uint)(KdenliveSettings::videotracksize());
	uint width = (uint)(height * 1.25);
	emit getClipThumbnail(fileURL(), (int)m_cropStart.frames(KdenliveSettings::defaultfps()), width, height);
}

void DocClipRef::fetchEndThumbnail()
{
	uint height = (uint)(KdenliveSettings::videotracksize());
	uint width = (uint)(height * 1.25);
	emit getClipThumbnail(fileURL(),(int) ( m_cropStart.frames(KdenliveSettings::defaultfps())+cropDuration().frames(KdenliveSettings::defaultfps()) - 1), width, height);
}

void DocClipRef::setCropStartTime(const GenTime & time)
{
    // Adjust all transitions
    if (!m_transitionStack.isEmpty()) {
        TransitionStack::iterator itt = m_transitionStack.begin();
        while (itt != m_transitionStack.end()) {
            if ((*itt)->transitionStartTime() != trackStart())
                (*itt)->moveTransition(m_cropStart - time);
            ++itt;
        }
    }
    m_cropStart = time;
    if (m_parentTrack) {
	m_parentTrack->clipMoved(this);
    }
}

void DocClipRef::moveCropStartTime(const GenTime & time)
{
    // Delete all transitions before new start time
    if (!m_transitionStack.isEmpty()) {
	GenTime cutTime = time + m_trackStart - m_cropStart;
        TransitionStack::iterator itt = m_transitionStack.begin();
        while (itt != m_transitionStack.end()) {
            if ((*itt)->transitionStartTime() < cutTime) {
                if ((*itt)->transitionEndTime() < cutTime) m_transitionStack.remove(*itt);
		else {
			GenTime transEnd = (*itt)->transitionEndTime();
			(*itt)->moveTransition(m_cropStart - time);
			(*itt)->resizeTransitionEnd(transEnd + m_cropStart - time);
		}
	    }
	    else (*itt)->moveTransition(m_cropStart - time);
            ++itt;
        }
    }
    m_cropStart = time;
    if (m_parentTrack) {
	m_parentTrack->clipMoved(this);
    }
}

const GenTime & DocClipRef::cropStartTime() const
{
    return m_cropStart;
    //return adjustTimeToSpeed(m_cropStart);
}


void DocClipRef::setTrackEnd(const GenTime & time)
{
    // Check all transitions
    if (!m_transitionStack.isEmpty()) {
        TransitionStack::iterator itt = m_transitionStack.begin();
        while (itt != m_transitionStack.end()) {
            if (time - (*itt)->transitionStartTime() < GenTime(2, m_clip->framesPerSecond()))
                (*itt)->moveTransition(time - trackEnd()); 
            //GenTime(0) - GenTime(3, m_clip->framesPerSecond()));
            ++itt;
        }
    }

    // do not allow clips below 0
    if (time > GenTime(0.0)) {
    	// If user tries to make clip of 0 frame, resize to 5 frames
    	if (time < m_trackStart || time == m_trackStart) m_trackEnd = m_trackStart + GenTime(5, m_clip->framesPerSecond());
    	else m_trackEnd = time;
    }
    else m_trackEnd = GenTime(5, m_clip->framesPerSecond());

    if (m_parentTrack) {
	m_parentTrack->clipMoved(this);
    }
}

GenTime DocClipRef::cropDuration() const
{
    return trackEnd() - trackStart();
}

void DocClipRef::updateThumbnail(int frame, QPixmap newpix)
{
    if (m_cropStart.frames(KdenliveSettings::defaultfps()) + cropDuration().frames(KdenliveSettings::defaultfps()) -1 == frame)
        m_endthumbnail = newpix;
    else if (m_cropStart.frames(KdenliveSettings::defaultfps()) == frame) m_thumbnail = newpix;
    else return;
    if (m_parentTrack) m_parentTrack->notifyClipChanged(this);
}


QPixmap DocClipRef::thumbnail(bool end)
{
    if (end) return m_endthumbnail;
    return m_thumbnail;
}

int DocClipRef::thumbnailWidth()
{
    return m_thumbnail.width();
}

DocClipRef *DocClipRef::
createClip(const EffectDescriptionList & effectList,
    ClipManager & clipManager, const QDomElement & element)
{
    DocClipRef *clip = 0;
    GenTime trackStart;
    GenTime cropStart;
    GenTime cropDuration;
    GenTime trackEnd;
    QString description;
    double speed = 1.0;
    double endspeed = 1.0;
    EffectStack effectStack;
    QValueVector < CommentedTime > markers;

    /*kdWarning() << "================================" << endl;
    kdWarning() << "Creating Clip : " << element.ownerDocument().
    toString() << endl;*/

    int trackNum = -1;

    QDomNode node = element;
    node.normalize();

    if (element.tagName() != "kdenliveclip") {
	kdWarning() <<
	    "DocClipRef::createClip() element has unknown tagName : " <<
	    element.tagName() << endl;
	return 0;
    }

    DocClipBase *baseClip = clipManager.insertClip(element);
    if (baseClip) {
	clip = new DocClipRef(baseClip);
    }

    QDomElement t;

    QDomNode n = element.firstChild();

    while (!n.isNull()) {
	QDomElement e = n.toElement();
	if (!e.isNull()) {
	    //kdWarning() << "DocClipRef::createClip() tag = " << e.tagName() << endl;
	    if (e.tagName() == "avfile") {
		// Do nothing, this is handled via the clipmanage insertClip method above.
	    } else if (e.tagName() == "project") {
		// Do nothing, this is handled via the clipmanage insertClip method above.
	    } else if (e.tagName() == "position") {
		trackNum = e.attribute("kdenlivetrack", "-1").toInt();
		trackStart =
		    GenTime(e.attribute("trackstart", "0").toDouble());
		cropStart =
		    GenTime(e.attribute("cropstart", "0").toDouble());
		cropDuration =
		    GenTime(e.attribute("cropduration", "0").toDouble());
		trackEnd =
		    GenTime(e.attribute("trackend", "-1").toDouble());
		speed = e.attribute("speed", "1.0").toDouble();
		endspeed = e.attribute("end_speed", "1.0").toDouble();
	    }  else if (e.tagName() == "effects") {
		//kdWarning() << "Found effects tag" << endl;
		QDomNode effectNode = e.firstChild();
		while (!effectNode.isNull()) {
		    QDomElement effectElement = effectNode.toElement();
		    //kdWarning() << "Effect node..." << endl;
		    if (!effectElement.isNull()) {
			//kdWarning() << "has tag name " << effectElement.tagName() << endl;
			if (effectElement.tagName() == "effect") {
			    EffectDesc *desc =
				effectList.effectDescription(effectElement.
				attribute("type", QString::null));
			    if (desc) {
				//kdWarning() << "Appending effect " <<desc->name() << endl;
				effectStack.append(Effect::createEffect(*desc, effectElement));
			    } else {
				kdWarning() << "Unknown effect " <<
				    effectElement.attribute("type", QString::null) << endl;
			    }
			} else {
			    kdWarning() << "Unknown tag " << effectElement.
				tagName() << endl;
			}
		    }
		    effectNode = effectNode.nextSibling();
		}
            }
            else if (e.tagName() == "transitions") {
                t = e;
                //kdWarning() << "Found transition tag" << endl;

            } else if (e.tagName() == "markers") {
		QDomNode markerNode = e.firstChild();
		while (!markerNode.isNull()) {
		    QDomElement markerElement = markerNode.toElement();
		    if (!markerElement.isNull()) {
			if (markerElement.tagName() == "marker") {
			    markers.append(CommentedTime(GenTime(markerElement.
				    attribute("time", "0").toDouble()),markerElement.attribute("comment", "")));
			} else {
			    kdWarning() << "Unknown tag " << markerElement.
				tagName() << endl;
			}
		    }
		    markerNode = markerNode.nextSibling();
		}
	    }
            else {
//               kdWarning() << "DocClipRef::createClip() unknown tag : " << e.tagName() << endl;
	    }
	}

	n = n.nextSibling();
    }

    if (clip == 0) {
	kdWarning() << "DocClipRef::createClip() unable to create clip" <<
	    endl;
    } else {
	// setup DocClipRef specifics of the clip.
	clip->setTrackStart(trackStart);
	clip->setCropStartTime(cropStart);
	if (clip->snapMarkers().count() == 0) clip->setSnapMarkers(markers);
	clip->setSpeed(speed, endspeed);
	if (trackEnd.seconds() != -1) {
	    clip->setTrackEnd(trackEnd);
	} else {
            clip->setTrackEnd(trackStart + cropDuration - GenTime(1, clip->framesPerSecond()));
	}
	clip->setParentTrack(0, trackNum);
	//clip->setDescription(description);
	clip->setEffectStack(effectStack);
        
        // add Transitions
        if (!t.isNull()) {
            QDomNode transitionNode = t.firstChild();
            while (!transitionNode.isNull()) {
                QDomElement transitionElement = transitionNode.toElement();
                if (!transitionElement.isNull()) {
                    if (transitionElement.tagName() == "ktransition") {
			Transition *transit = new Transition(clip, transitionElement);
                        clip->addTransition(transit);
                    } else {
                        kdWarning() << "Unknown transition " <<transitionElement.attribute("type",QString::null) << endl;
                    }
                } else {
                kdWarning() << "Unknown tag " << transitionElement.tagName() << endl;
                }
                transitionNode = transitionNode.nextSibling();
            }
        }
    }
    return clip;
}

/** Sets the parent track for this clip. */
void DocClipRef::setParentTrack(DocTrackBase * track, const int trackNum)
{
    m_parentTrack = track;
    m_trackNum = trackNum;
}

/** Returns the track number. This is a hint as to which track the clip is on, or should be placed on. */
int DocClipRef::trackNum() const
{
    return m_trackNum;
}

/** Returns the track number in MLT's playlist */
int DocClipRef::playlistTrackNum() const
{
    return m_parentTrack->projectClip()->playlistTrackNum(m_trackNum);
}

/** Returns the track number in MLT's playlist */
int DocClipRef::playlistNextTrackNum() const
{
    return m_parentTrack->projectClip()->playlistNextVideoTrack(m_trackNum);
}

/** Returns the track number in MLT's playlist */
int DocClipRef::playlistOtherTrackNum(int num) const
{
    return m_parentTrack->projectClip()->playlistTrackNum(num);
}

/** Returns the end of the clip on the track. A convenience function, equivalent
to trackStart() + cropDuration() */
GenTime DocClipRef::trackEnd() const
{
    return m_trackEnd;
    //return adjustTimeToSpeed(m_trackEnd - m_trackStart + m_cropStart) - cropStartTime() + m_trackStart;
}

/** Returns the parentTrack of this clip. */
DocTrackBase *DocClipRef::parentTrack()
{
    return m_parentTrack;
}

/** Move the clips so that it's trackStart coincides with the time specified. */
void DocClipRef::moveTrackStart(const GenTime & time)
{
    m_trackEnd = m_trackEnd + time - m_trackStart;
    m_trackStart = time;
}

DocClipRef *DocClipRef::clone(const EffectDescriptionList & effectList,
    ClipManager & clipManager)
{
    return createClip(effectList, clipManager, toXML().documentElement());
}

bool DocClipRef::referencesClip(DocClipBase * clip) const
{
    return m_clip->referencesClip(clip);
}

uint DocClipRef::numReferences() const
{
    return m_clip->numReferences();
}

double DocClipRef::framesPerSecond() const
{
    if (m_parentTrack) {
	return m_parentTrack->framesPerSecond();
    } else {
	return m_clip->framesPerSecond();
    }
}

//returns clip video properties -reh
DocClipBase::CLIPTYPE DocClipRef::clipType() const
{
    return m_clip->clipType();
}

uint DocClipRef::clipHeight() const
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->clipHeight();
    else if (m_clip->isDocClipTextFile())    
        return m_clip->toDocClipTextFile()->clipHeight();
    return KdenliveSettings::defaultheight();
}

uint DocClipRef::clipWidth() const
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->clipWidth();
    else if (m_clip->isDocClipTextFile())
        return m_clip->toDocClipTextFile()->clipWidth();
    return KdenliveSettings::defaultwidth();
}

QString DocClipRef::avDecompressor()
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->avDecompressor();
    return QString::null;
}

//type ntsc/pal
QString DocClipRef::avSystem()
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->avSystem();
    return QString::null;
}

//returns clip audio properties -reh
uint DocClipRef::audioChannels() const
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->audioChannels();
    return 0;
}

QString DocClipRef::audioFormat()
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->audioFormat();
    return QString::null;
}

uint DocClipRef::audioBits() const
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->audioBits();
    return 0;
}

uint DocClipRef::audioFrequency() const
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->audioFrequency();
    return 0;
}

QDomDocument DocClipRef::toXML() const
{
    QDomDocument doc = m_clip->toXML();


    QDomElement clip = doc.documentElement();

    if (clip.tagName() != "kdenliveclip") {
	kdError() <<
	    "Expected tagname of 'clip' in DocClipRef::toXML(), expect things to go wrong!"
	    << endl;
    }

    QDomElement position = doc.createElement("position");
    position.setAttribute("kdenlivetrack", QString::number(trackNum()));
    position.setAttribute("trackstart",
	QString::number(trackStart().seconds(), 'f', 10));
    position.setAttribute("cropstart",
	QString::number(cropStartTime().seconds(), 'f', 10));
    position.setAttribute("cropduration",
	QString::number(cropDuration().seconds(), 'f', 10));
    position.setAttribute("trackend", QString::number(trackEnd().seconds(),
	    'f', 10));
    position.setAttribute("speed", QString::number(m_speed, 'f'));
    position.setAttribute("end_speed", QString::number(m_endspeed, 'f'));

    clip.appendChild(position);

    //  append clip effects
    if (!m_effectStack.isEmpty()) {
	QDomElement effects = doc.createElement("effects");

	EffectStack::iterator itt = m_effectStack.begin();
	while (itt != m_effectStack.end()) {
	    effects.appendChild(doc.importNode((*itt)->toXML().
		    documentElement(), true));
	    ++itt;
	}

	clip.appendChild(effects);
    }
    
    //  append clip transitions
    if (!m_transitionStack.isEmpty()) {
        QDomElement trans = doc.createElement("transitions");

        TransitionStack::iterator itt = m_transitionStack.begin();
        while (itt != m_transitionStack.end()) {
            trans.appendChild(doc.importNode((*itt)->toXML(), true));
            ++itt;
        }

        clip.appendChild(trans);
    }
    doc.appendChild(clip);

    return doc;
}

QStringList DocClipRef::clipEffectNames()
{
    QStringList effectNames;
    if (!m_effectStack.isEmpty()) {
	EffectStack::iterator itt = m_effectStack.begin();
	while (itt != m_effectStack.end()) {
	    if ((*itt)->isEnabled()) effectNames<<(*itt)->name();
	    ++itt;
	}
    }
    return effectNames;
}

bool DocClipRef::matchesXML(const QDomElement & element) const
{
    bool result = false;
    if (element.tagName() == "kdenliveclip") {
	QDomNodeList nodeList = element.elementsByTagName("position");
	if (nodeList.length() > 0) {
	    if (nodeList.length() > 1) {
		kdWarning() <<
		    "More than one position in XML for a docClip? Only matching first one"
		    << endl;
	    }
	    QDomElement positionElement = nodeList.item(0).toElement();

	    if (!positionElement.isNull()) {
		result = true;
		if (positionElement.attribute("kdenlivetrack").toInt() !=
		    trackNum())
		    result = false;
		if (positionElement.attribute("trackstart").toInt() !=
		    trackStart().seconds())
		    result = false;
		if (positionElement.attribute("cropstart").toInt() !=
		    cropStartTime().seconds())
		    result = false;
		if (positionElement.attribute("cropduration").toInt() !=
		    cropDuration().seconds())
		    result = false;
		if (positionElement.attribute("trackend").toInt() !=
		    trackEnd().seconds())
		    result = false;
		if (positionElement.attribute("speed").toDouble() !=
		    speed())
		    result = false;

	    }
	}
    }

    return result;
}

const GenTime & DocClipRef::duration() const
{
    return m_clip->duration();
    /*if (m_speed == 1.0 && m_endspeed == 1.0) return m_clip->duration();
    else {
	int frameCount = (int)(m_clip->duration().frames(framesPerSecond()) * 2 / (m_speed + m_endspeed));
	return GenTime(frameCount, framesPerSecond());
    }*/
}

bool DocClipRef::durationKnown() const
{
    return m_clip->durationKnown();
}

double DocClipRef::speed() const
{
    return (m_speed + m_endspeed) / 2;
}

void DocClipRef::setSpeed(double startspeed, double endspeed)
{
    if (!m_parentTrack) return;
    double origDuration = (double)((m_trackEnd.frames(framesPerSecond()) - m_trackStart.frames(framesPerSecond()))) * speed();

    double newDuration = origDuration * 2.0 / (startspeed + endspeed);
    if (trackEnd() != m_parentTrack->trackLength()) {
	// The clip is not the last one on track, check available space
        GenTime availableSpace = GenTime(0) - m_parentTrack->spaceLength(trackEnd() + GenTime(1, framesPerSecond()));
        if (GenTime(newDuration, framesPerSecond()) - cropDuration() > availableSpace) {
	    kdDebug()<<"// Cannot change speed, it would overlap another clip."<<endl;
	    if (!m_effectStack.isEmpty()) {
		EffectStack::iterator itt = m_effectStack.begin();
		while (itt != m_effectStack.end()) {
	    		if ((*itt)->name() == i18n("Speed")) {
			    (*itt)->effectDescription().parameter(0)->setValue(QString::number((int) (m_speed * 100)));
			    (*itt)->effectDescription().parameter(1)->setValue(QString::number((int) (m_endspeed * 100)));
			}
	    		++itt;
		}
            }
	    return;
	}
    }
    m_speed = startspeed;
    m_endspeed = endspeed;
    m_trackEnd = m_trackStart + GenTime(newDuration, framesPerSecond());
    if (m_parentTrack) m_parentTrack->notifyTrackChanged(this);
}

QDomDocument DocClipRef::generateSceneList()
{
    return m_clip->generateSceneList();
}

QDomDocumentFragment DocClipRef::generateXMLTransition(bool hideVideo, bool hideAudio)
{
    QDomDocument transitionList;
    QDomDocumentFragment list = transitionList.createDocumentFragment();

    transitionList.appendChild(list);
    DocClipBase::CLIPTYPE ct = clipType();
    bool transparentBackgroundClip = false;

    if (!hideVideo) {
	if (ct == DocClipBase::TEXT && m_clip->toDocClipTextFile()->isTransparent()) 
	    transparentBackgroundClip = true;
	else if ((ct == DocClipBase::IMAGE || ct == DocClipBase::SLIDESHOW) && m_clip->toDocClipAVFile()->isTransparent()) 
	    transparentBackgroundClip = true;
    }

    // Add transition to create transparent bg effect for image/text clips
    if (transparentBackgroundClip)
    {
	int transitionNumber = m_transitionStack.count();
	QValueList < QPoint > blanklist;
	blanklist.append(QPoint((int) trackStart().frames(framesPerSecond()), (int) trackEnd().frames(framesPerSecond()) - 1));
	while (transitionNumber > 0) {
	    // Parse all clip transitions and build a list of times without transitions
	    QValueList < QPoint >::Iterator it;
	    Transition *t = m_transitionStack.at(transitionNumber - 1);
	    int transStart = (int) t->transitionStartTime().frames(framesPerSecond());
	    int transEnd = (int) t->transitionEndTime().frames(framesPerSecond()) - 1;
	    for ( it = blanklist.begin(); it != blanklist.end(); ++it ) {
		int currentStart = (*it).x();
		int currentEnd = (*it).y();
		if (transStart <= currentEnd && transEnd >= currentStart) {
		    
		    if (currentStart != transStart) {
			(*it) = QPoint(currentStart, transStart);
			if (transEnd != currentEnd) blanklist.append(QPoint(transEnd, currentEnd));
		    }
		    else (*it) = QPoint(transEnd, currentEnd);
		    break;
		}
	    }
	    transitionNumber--;
	}

	// Insert "transparent" composite transition in places where the clip has no transition
	QValueList < QPoint >::Iterator it;
 	for ( it = blanklist.begin(); it != blanklist.end(); ++it ) {
            QDomElement transition = transitionList.createElement("transition");
            transition.setAttribute("in", (*it).x());
            transition.setAttribute("out", (*it).y());
            transition.setAttribute("mlt_service", "composite");
            transition.setAttribute("fill", "1");
            transition.setAttribute("progressive","1");
            transition.setAttribute("a_track", QString::number( playlistNextTrackNum()));
            transition.setAttribute("b_track", QString::number(playlistTrackNum()));
            list.appendChild(transition);
	}
    }
 
    TransitionStack::iterator itt = m_transitionStack.begin(); 
    while (itt) {
	bool block = false;
	uint type = (*itt)->transitionType();
	if ((type < 100) &&  m_parentTrack->clipType() == "Sound")
	    block = true;
	if (hideVideo && type < 200) block = true;
	if (hideAudio && type > 99) block = true;
	if (!block) {
            QDomElement transition = transitionList.createElement("transition");
            transition.setAttribute("in", QString::number((*itt)->transitionStartTime().frames(framesPerSecond())));
            transition.setAttribute("out", QString::number((*itt)->transitionEndTime().frames(framesPerSecond()) - 1));
            if (type == Transition::PIP_TRANSITION) transition.setAttribute("mlt_service", "composite");
            else transition.setAttribute("mlt_service", (*itt)->transitionTag());
            transition.setAttribute("fill", "1");
            //transition.setAttribute("distort", "1");

	    // Parse transition attributes
            typedef QMap<QString, QString> ParamMap;
            ParamMap params;
            params = (*itt)->transitionParameters();
            ParamMap::Iterator it;
            for ( it = params.begin(); it != params.end(); ++it ) {
            	transition.setAttribute(it.key(), it.data());
            }

	    /* The crossfade LUMA transition does not work on images/texts with transp. 
	       background, so we replace it with a composite transition */
	    if (transparentBackgroundClip && type == Transition::LUMA_TRANSITION) {
		transition.setAttribute("mlt_service", "composite");
		QString geom;
		if (!(*itt)->invertTransition()) geom = "0=0,0:100%x100%:0;-1=0,0:100%x100%:100";
		else geom = "0=0,0:100%x100%:100;-1=0,0:100%x100%:0";
		transition.setAttribute("geometry", geom);
	    }

	    if (type == Transition::LUMA_TRANSITION || type == Transition::MIX_TRANSITION || type == Transition::LUMAFILE_TRANSITION) {
                transition.setAttribute("b_track", QString::number((*itt)->transitionStartTrack()));
                transition.setAttribute("a_track", QString::number((*itt)->transitionEndTrack()));
	        if ((*itt)->invertTransition()) transition.setAttribute("reverse", "1");
	    }
	    else if ((*itt)->invertTransition()) {
                transition.setAttribute("a_track", QString::number((*itt)->transitionStartTrack()));
                transition.setAttribute("b_track", QString::number((*itt)->transitionEndTrack()));
            }
            else {
                transition.setAttribute("b_track", QString::number((*itt)->transitionStartTrack()));
                transition.setAttribute("a_track", QString::number((*itt)->transitionEndTrack()));
            }
            list.appendChild(transition);
	}
        ++itt;
    }
    return list;
}


QDomDocumentFragment DocClipRef::generateOffsetXMLTransition(bool hideVideo, bool hideAudio, GenTime start, GenTime end)
{
    QDomDocument transitionList;
    QDomDocumentFragment list = transitionList.createDocumentFragment();

    transitionList.appendChild(list);
    DocClipBase::CLIPTYPE ct = clipType();
    bool transparentBackgroundClip = false;

    if (!hideVideo) {
	if (ct == DocClipBase::TEXT && m_clip->toDocClipTextFile()->isTransparent()) 
	transparentBackgroundClip = true;
    else if ((ct == DocClipBase::IMAGE || ct == DocClipBase::SLIDESHOW) && m_clip->toDocClipAVFile()->isTransparent()) 
	transparentBackgroundClip = true;
    }

    // Add transition to create transparent bg effect for image/text clips
    if (transparentBackgroundClip && m_transitionStack.count() == 0)
    {
        QDomElement transition = transitionList.createElement("transition");
        transition.setAttribute("in", (trackStart() - start).frames(framesPerSecond()));
        transition.setAttribute("out", (trackEnd() - start).frames(framesPerSecond()) - 1);
        transition.setAttribute("mlt_service", "composite");
        transition.setAttribute("fill", "1");
        transition.setAttribute("progressive","1");
        transition.setAttribute("a_track", QString::number( playlistNextTrackNum()));
        transition.setAttribute("b_track", QString::number(playlistTrackNum()));
        list.appendChild(transition);
    }
 
    TransitionStack::iterator itt = m_transitionStack.begin(); 
    while (itt) {
	bool block = false;
	uint type = (*itt)->transitionType();
	if ((type < 100) &&  m_parentTrack->clipType() == "Sound")
	    block = true;
	if (hideVideo && type < 200) block = true;
	if (hideAudio && type > 99) block = true;
	if (!block) {
            QDomElement transition = transitionList.createElement("transition");
            transition.setAttribute("in", QString::number(((*itt)->transitionStartTime() - start).frames(framesPerSecond())));
            transition.setAttribute("out", QString::number(((*itt)->transitionEndTime() - start).frames(framesPerSecond()) - 1));

            if (type == Transition::PIP_TRANSITION) transition.setAttribute("mlt_service", "composite");
            else transition.setAttribute("mlt_service", (*itt)->transitionTag());
            transition.setAttribute("fill", "1");
            //transition.setAttribute("distort", "1");

	    // Parse transition attributes
            typedef QMap<QString, QString> ParamMap;
            ParamMap params;
            params = (*itt)->transitionParameters();
            ParamMap::Iterator it;
            for ( it = params.begin(); it != params.end(); ++it ) {
            	transition.setAttribute(it.key(), it.data());
            }

	    /* The crossfade LUMA transition does not work on images/texts with transp. 
	       background, so we replace it with a composite transition */
	    if (transparentBackgroundClip && type == Transition::LUMA_TRANSITION) {
		transition.setAttribute("mlt_service", "composite");
		QString geom;
		if (!(*itt)->invertTransition()) geom = "0=0,0:100%x100%:0;-1=0,0:100%x100%:100";
		else geom = "0=0,0:100%x100%:100;-1=0,0:100%x100%:0";
		transition.setAttribute("geometry", geom);
	    }

	    if (type == Transition::LUMA_TRANSITION || type == Transition::MIX_TRANSITION || type == Transition::LUMAFILE_TRANSITION) {
                transition.setAttribute("b_track", QString::number((*itt)->transitionStartTrack()));
                transition.setAttribute("a_track", QString::number((*itt)->transitionEndTrack()));
	        if ((*itt)->invertTransition()) transition.setAttribute("reverse", "1");
	    }
	    else if ((*itt)->invertTransition()) {
                transition.setAttribute("a_track", QString::number((*itt)->transitionStartTrack()));
                transition.setAttribute("b_track", QString::number((*itt)->transitionEndTrack()));
            }
            else {
                transition.setAttribute("b_track", QString::number((*itt)->transitionStartTrack()));
                transition.setAttribute("a_track", QString::number((*itt)->transitionEndTrack()));
            }
            list.appendChild(transition);
	}
        ++itt;
    }
    return list;
}


QDomDocument DocClipRef::generateXMLClip()
{
    if (m_cropStart == m_trackEnd)
	return QDomDocument();

    QDomDocument sceneList;
 
    QDomElement entry;

     //if (m_speed == 1.0) 
    {
	entry = sceneList.createElement("entry");
    	entry.setAttribute("producer", m_clip->getId());
    }

    if (m_clip->clipType() == DocClipBase::SLIDESHOW && m_clip->toDocClipAVFile()->hasCrossfade()) {
    	QDomElement clipFilter =
	sceneList.createElement("filter");
        clipFilter.setAttribute("mlt_service", "luma");
	clipFilter.setAttribute("period", QString::number(m_clip->toDocClipAVFile()->clipTtl() - 1));
	entry.appendChild(clipFilter);
    }

    // Check if clip is positionned under 0 in the timeline
	 int checkStart = (int)(m_trackStart.frames(framesPerSecond()));
    if (checkStart < 0)
        entry.setAttribute("in", QString::number(m_cropStart.frames(framesPerSecond()) - checkStart));
    else 
        entry.setAttribute("in", QString::number(m_cropStart.frames(framesPerSecond())));
        entry.setAttribute("out", QString::number((m_cropStart + cropDuration()).frames(framesPerSecond()) - 1));
    
    // Generate XML for the clip's effects 
    // As a starting point, let's consider effects don't have more than one keyframable parameter.
    // All other parameters are supposed to be "constant", ie a value which can be adjusted by 
    // the user but remains the same during all the clip's duration.

    QDomElement monoFilter = sceneList.createElement("filter");
    monoFilter.setAttribute("mlt_service", "channelcopy");

    uint i = 0;
    if (hasEffect())
	while (effectAt(i) != NULL) {
	    Effect *effect = effectAt(i);
	    if (effect->isEnabled()) {
	    uint parameterNum = 0;
	   // bool hasParameters = false;

	    if (effect->effectDescription().tag().startsWith("ladspa", false)) {
		// THIS is a LADSPA FILTER, process 
                        QDomElement clipFilter = sceneList.createElement("filter");
			clipFilter.setAttribute("mlt_service", "ladspa");
			QStringList params;


			while (effect->parameter(parameterNum)) {
			    if (effect->effectDescription().parameter(parameterNum)->type() == "constant") {
				double effectParam;
			    	if (effect->effectDescription().parameter(parameterNum)->factor() != 1.0)
                            		effectParam = effect->effectDescription().parameter(parameterNum)->value().toDouble() / effect->effectDescription().parameter(parameterNum)->factor();
			    	else effectParam = effect->effectDescription().parameter(parameterNum)->value().toDouble();
				params.append(QString::number(effectParam));
			    }
			    else params.append(effect->effectDescription().parameter(parameterNum)->value());
			    parameterNum++;
			}
			int ladspaid = effect->effectDescription().tag().right(effect->effectDescription().tag().length() - 6).toInt();

			QString effectFile = effect->tempFileName();
			if (!effectFile.isEmpty()) {
			    initEffects::ladspaEffectFile( effectFile, ladspaid, params );
			    clipFilter.setAttribute("src", effectFile );
			}
//			clipFilter.setAttribute("data", initEffects::ladspaEffectString(ladspaid, params ));
		    	entry.appendChild(clipFilter);
			if (effect->effectDescription().isMono()) {
			    // Audio Mono clip. Duplicate audio channel
			    entry.appendChild(monoFilter);
			}

		// end of LADSPA FILTER

	    }
	    else while (effect->parameter(parameterNum)) {
		uint keyFrameNum = 0;

		if (effect->effectDescription().parameter(parameterNum)->type() == "complex") {
		    // Effect has keyframes with several sub-parameters
		    QString startTag, endTag;
		    keyFrameNum =
			effect->parameter(parameterNum)->numKeyFrames();
		    startTag =
			effect->effectDescription().
			parameter(parameterNum)->startTag();
		    endTag =
			effect->effectDescription().
			parameter(parameterNum)->endTag();

		    if (keyFrameNum > 1) {
			for (uint count = 0; count < keyFrameNum - 1;
			    count++) {
			    QDomElement transition =
				sceneList.createElement("filter");
			    transition.setAttribute("mlt_service",
				effect->effectDescription().tag());
			    uint in =
				(uint)(m_cropStart.frames(framesPerSecond()));
			    uint duration =
				(uint)(cropDuration().frames(framesPerSecond()));
			    transition.setAttribute("in",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count)->time()) *
				    duration));
			    transition.setAttribute("out",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count +
					    1)->time()) * duration));

			    transition.setAttribute(startTag,
				effect->parameter(parameterNum)->
				keyframe(count)->toComplexKeyFrame()->
				processComplexKeyFrame());

			    transition.setAttribute(endTag,
				effect->parameter(parameterNum)->
				keyframe(count +
				    1)->toComplexKeyFrame()->
				processComplexKeyFrame());
			    entry.appendChild(transition);
			}
		    }
		}

		else if (effect->effectDescription().
		    parameter(parameterNum)->type() == "double") {
		    // Effect has one parameter with keyframes
		    QString startTag, endTag;
		    keyFrameNum =
			effect->parameter(parameterNum)->numKeyFrames();
		    startTag =
			effect->effectDescription().
			parameter(parameterNum)->startTag();
		    endTag =
			effect->effectDescription().
			parameter(parameterNum)->endTag();

		    double factor = effect->effectDescription().parameter(parameterNum)->factor();

		    if (keyFrameNum > 1) {
			for (uint count = 0; count < keyFrameNum - 1;
			    count++) {
                                QDomElement clipFilter =
				sceneList.createElement("filter");
			    clipFilter.setAttribute("mlt_service",
				effect->effectDescription().tag());
				 uint in =(uint)
				m_cropStart.frames(framesPerSecond());
				 uint duration =(uint)
				cropDuration().frames(framesPerSecond());
			    clipFilter.setAttribute("in",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count)->time()) *
				    duration));
			    clipFilter.setAttribute(startTag,
				QString::number(effect->
				    parameter(parameterNum)->
				    keyframe(count)->toDoubleKeyFrame()->
				    value() / factor));
			    clipFilter.setAttribute("out",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count +
					    1)->time()) * duration));
			    clipFilter.setAttribute(endTag,
				QString::number(effect->
				    parameter(parameterNum)->
				    keyframe(count +
					1)->toDoubleKeyFrame()->value() / factor));

			    // Add the other constant parameters if any
			    /*uint parameterNumBis = parameterNum;
			       while (effect->parameter(parameterNumBis)) {
			       transition.setAttribute(effect->effectDescription().parameter(parameterNumBis)->name(),QString::number(effect->effectDescription().parameter(parameterNumBis)->value()));
			       parameterNumBis++;
			       } */
			    entry.appendChild(clipFilter);
			}
		    }
                    }
                else {	// Effect has only constant parameters
		    if (effect->effectDescription().tag() != "framebuffer") {
                    QDomElement clipFilter =
			sceneList.createElement("filter");
                    clipFilter.setAttribute("mlt_service",
			effect->effectDescription().tag());
		    if (effect->effectDescription().
			parameter(parameterNum)->type() == "constant" || effect->effectDescription().
			parameter(parameterNum)->type() == "list" || effect->effectDescription().
			parameter(parameterNum)->type() == "bool")
			while (effect->parameter(parameterNum)) {
			    if (effect->effectDescription().parameter(parameterNum)->factor() != 1.0)
                            clipFilter.setAttribute(effect->
				effectDescription().
				parameter(parameterNum)->name(), QString::number(
				effect->
				    effectDescription().
				    parameter(parameterNum)->value().toDouble() / effect->
				    effectDescription().
				    parameter(parameterNum)->factor()));
			    else clipFilter.setAttribute(effect->
				effectDescription().
				parameter(parameterNum)->name(), effect->effectDescription().parameter(parameterNum)->value());
			    parameterNum++;
			}
                        entry.appendChild(clipFilter);
		    }
		    else {  //slowmotion or freeze effect, use special producer
    				entry.setTagName("producer");
    				entry.setAttribute("mlt_service","framebuffer");
    				entry.setAttribute("id","slowmotion"+ QString::number(m_clip->getId()));
				QString fileName;
				if (effect->effectDescription().name() == i18n("Speed")) {
				    fileName = fileURL().path() + ":" + QString::number(effect->effectDescription().parameter(0)->value().toDouble() / effect->effectDescription().parameter(0)->factor()) + ":" + QString::number(effect->effectDescription().parameter(1)->value().toDouble() / effect->effectDescription().parameter(1)->factor());
				}
				else fileName = fileURL().path();
    				entry.setAttribute("resource", fileName.ascii());
    				entry.removeAttribute("producer");
				while (effect->parameter(parameterNum)) {
					entry.setAttribute(effect->effectDescription().parameter(parameterNum)->name(), QString::number(effect->effectDescription().parameter(parameterNum)->value().toDouble() / effect->effectDescription().parameter(parameterNum)->factor()));
			    		parameterNum++;
				}
		    }
		}
		parameterNum++;
	    }
	    }
	    i++;
	}

    sceneList.appendChild(entry);
    return sceneList;
    //return m_clip->toDocClipAVFile()->sceneToXML(m_cropStart, m_cropStart+cropDuration());        
}


QDomDocument DocClipRef::generateOffsetXMLClip(GenTime start, GenTime end)
{
    if (m_cropStart == m_trackEnd)
	return QDomDocument();

    QDomDocument sceneList;
    QDomElement entry;

     //if (m_speed == 1.0) 
    {
	entry = sceneList.createElement("entry");
    	entry.setAttribute("producer", m_clip->getId());
    }

    if (m_clip->clipType() == DocClipBase::SLIDESHOW && m_clip->toDocClipAVFile()->hasCrossfade()) {
    	QDomElement clipFilter =
	sceneList.createElement("filter");
        clipFilter.setAttribute("mlt_service", "luma");
	clipFilter.setAttribute("period", QString::number(m_clip->toDocClipAVFile()->clipTtl() - 1));
	entry.appendChild(clipFilter);
    }

    // Check if clip is positionned under 0 in the timeline
	 int checkStart = (int)((m_trackStart - start).frames(framesPerSecond()));
    if (checkStart < 0)
        entry.setAttribute("in", QString::number(m_cropStart.frames(framesPerSecond()) - checkStart));
    else 
        entry.setAttribute("in", QString::number(m_cropStart.frames(framesPerSecond())));
	int checkEnd = (int)((m_trackEnd - end).frames(framesPerSecond()));
	if (checkEnd > 0)
	    entry.setAttribute("out", QString::number((m_cropStart + cropDuration()).frames(framesPerSecond()) - 1 - checkEnd));
	else entry.setAttribute("out", QString::number((m_cropStart + cropDuration()).frames(framesPerSecond()) - 1));
    
    // Generate XML for the clip's effects 
    // As a starting point, let's consider effects don't have more than one keyframable parameter.
    // All other parameters are supposed to be "constant", ie a value which can be adjusted by 
    // the user but remains the same during all the clip's duration.

    QDomElement monoFilter = sceneList.createElement("filter");
    monoFilter.setAttribute("mlt_service", "channelcopy");

    uint i = 0;
    if (hasEffect())
	while (effectAt(i) != NULL) {
	    Effect *effect = effectAt(i);
	    if (effect->isEnabled()) {
	    uint parameterNum = 0;
	   // bool hasParameters = false;

	    if (effect->effectDescription().tag().startsWith("ladspa", false)) {
		// THIS is a LADSPA FILTER, process 
                        QDomElement clipFilter = sceneList.createElement("filter");
			clipFilter.setAttribute("mlt_service", "ladspa");
			QStringList params;


			while (effect->parameter(parameterNum)) {
			    if (effect->effectDescription().parameter(parameterNum)->type() == "constant") {
				double effectParam;
			    	if (effect->effectDescription().parameter(parameterNum)->factor() != 1.0)
                            		effectParam = effect->effectDescription().parameter(parameterNum)->value().toDouble() / effect->effectDescription().parameter(parameterNum)->factor();
			    	else effectParam = effect->effectDescription().parameter(parameterNum)->value().toDouble();
				params.append(QString::number(effectParam));
			    }
			    else params.append(effect->effectDescription().parameter(parameterNum)->value());
			    parameterNum++;
			}
			int ladspaid = effect->effectDescription().tag().right(effect->effectDescription().tag().length() - 6).toInt();

			QString effectFile = effect->tempFileName();
			if (!effectFile.isEmpty()) {
			    initEffects::ladspaEffectFile( effectFile, ladspaid, params );
			    clipFilter.setAttribute("src", effectFile );
			}
//			clipFilter.setAttribute("data", initEffects::ladspaEffectString(ladspaid, params ));
		    	entry.appendChild(clipFilter);
			if (effect->effectDescription().isMono()) entry.appendChild(monoFilter);

		// end of LADSPA FILTER

	    }
	    else while (effect->parameter(parameterNum)) {
		uint keyFrameNum = 0;

		if (effect->effectDescription().parameter(parameterNum)->type() == "complex") {
		    // Effect has keyframes with several sub-parameters
		    QString startTag, endTag;
		    keyFrameNum =
			effect->parameter(parameterNum)->numKeyFrames();
		    startTag =
			effect->effectDescription().
			parameter(parameterNum)->startTag();
		    endTag =
			effect->effectDescription().
			parameter(parameterNum)->endTag();

		    if (keyFrameNum > 1) {
			for (uint count = 0; count < keyFrameNum - 1;
			    count++) {
			    QDomElement transition =
				sceneList.createElement("filter");
			    transition.setAttribute("mlt_service",
				effect->effectDescription().tag());
			    uint in =
				(uint)(m_cropStart.frames(framesPerSecond()));
			    uint duration =
				(uint)(cropDuration().frames(framesPerSecond()));
			    transition.setAttribute("in",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count)->time()) *
				    duration));
			    transition.setAttribute("out",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count +
					    1)->time()) * duration));

			    transition.setAttribute(startTag,
				effect->parameter(parameterNum)->
				keyframe(count)->toComplexKeyFrame()->
				processComplexKeyFrame());

			    transition.setAttribute(endTag,
				effect->parameter(parameterNum)->
				keyframe(count +
				    1)->toComplexKeyFrame()->
				processComplexKeyFrame());
			    entry.appendChild(transition);
			}
		    }
		}

		else if (effect->effectDescription().
		    parameter(parameterNum)->type() == "double") {
		    // Effect has one parameter with keyframes
		    QString startTag, endTag;
		    keyFrameNum =
			effect->parameter(parameterNum)->numKeyFrames();
		    startTag =
			effect->effectDescription().
			parameter(parameterNum)->startTag();
		    endTag =
			effect->effectDescription().
			parameter(parameterNum)->endTag();

		    double factor = effect->effectDescription().parameter(parameterNum)->factor();

		    if (keyFrameNum > 1) {
			for (uint count = 0; count < keyFrameNum - 1;
			    count++) {
                                QDomElement clipFilter =
				sceneList.createElement("filter");
			    clipFilter.setAttribute("mlt_service",
				effect->effectDescription().tag());
				 uint in =(uint)
				m_cropStart.frames(framesPerSecond());
				 uint duration =(uint)
				cropDuration().frames(framesPerSecond());
			    clipFilter.setAttribute("in",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count)->time()) *
				    duration));
			    clipFilter.setAttribute(startTag,
				QString::number(effect->
				    parameter(parameterNum)->
				    keyframe(count)->toDoubleKeyFrame()->
				    value() / factor));
			    clipFilter.setAttribute("out",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count +
					    1)->time()) * duration));
			    clipFilter.setAttribute(endTag,
				QString::number(effect->
				    parameter(parameterNum)->
				    keyframe(count +
					1)->toDoubleKeyFrame()->value() / factor));

			    // Add the other constant parameters if any
			    /*uint parameterNumBis = parameterNum;
			       while (effect->parameter(parameterNumBis)) {
			       transition.setAttribute(effect->effectDescription().parameter(parameterNumBis)->name(),QString::number(effect->effectDescription().parameter(parameterNumBis)->value()));
			       parameterNumBis++;
			       } */
			    entry.appendChild(clipFilter);
			}
		    }
                    }
                else {	// Effect has only constant parameters
		    if (effect->effectDescription().tag() != "framebuffer") {
                    QDomElement clipFilter =
			sceneList.createElement("filter");
                    clipFilter.setAttribute("mlt_service",
			effect->effectDescription().tag());
		    if (effect->effectDescription().
			parameter(parameterNum)->type() == "constant" || effect->effectDescription().
			parameter(parameterNum)->type() == "list" || effect->effectDescription().
			parameter(parameterNum)->type() == "bool")
			while (effect->parameter(parameterNum)) {
			    if (effect->effectDescription().parameter(parameterNum)->factor() != 1.0)
                            clipFilter.setAttribute(effect->
				effectDescription().
				parameter(parameterNum)->name(), QString::number(
				effect->
				    effectDescription().
				    parameter(parameterNum)->value().toDouble() / effect->
				    effectDescription().
				    parameter(parameterNum)->factor()));
			    else clipFilter.setAttribute(effect->
				effectDescription().
				parameter(parameterNum)->name(), effect->effectDescription().parameter(parameterNum)->value());
			    parameterNum++;
			}
                        entry.appendChild(clipFilter);
		    }
		    else {  //slowmotion or freeze effect, use special producer
    				entry.setTagName("producer");
    				entry.setAttribute("mlt_service","framebuffer");
    				entry.setAttribute("id","slowmotion"+ QString::number(m_clip->getId()));
				QString fileName;
				if (effect->effectDescription().name() == i18n("Speed")) {
				    fileName = fileURL().path() + ":" + QString::number(effect->effectDescription().parameter(0)->value().toDouble() / effect->effectDescription().parameter(0)->factor()) + ":" + QString::number(effect->effectDescription().parameter(1)->value().toDouble() / effect->effectDescription().parameter(1)->factor());
				}
				else fileName = fileURL().path();
    				entry.setAttribute("resource", fileName.ascii());
    				entry.removeAttribute("producer");
				while (effect->parameter(parameterNum)) {
					entry.setAttribute(effect->effectDescription().parameter(parameterNum)->name(), QString::number(effect->effectDescription().parameter(parameterNum)->value().toDouble() / effect->effectDescription().parameter(parameterNum)->factor()));
			    		parameterNum++;
				}
		    }
		}
		parameterNum++;
	    }
	    }
	    i++;
	}

    sceneList.appendChild(entry);
    return sceneList;
    //return m_clip->toDocClipAVFile()->sceneToXML(m_cropStart, m_cropStart+cropDuration());        
}


const KURL & DocClipRef::fileURL() const
{
    return m_clip->fileURL();
}

uint DocClipRef::fileSize() const
{
    return m_clip->fileSize();
}

void DocClipRef::populateSceneTimes(QValueVector < GenTime > &toPopulate)
{
    QValueVector < GenTime > sceneTimes;

    m_clip->populateSceneTimes(sceneTimes);

    QValueVector < GenTime >::iterator itt = sceneTimes.begin();
    while (itt != sceneTimes.end()) {
	GenTime convertedTime = (*itt) - cropStartTime();
	if ((convertedTime >= GenTime(0))
	    && (convertedTime <= cropDuration())) {
	    convertedTime += trackStart();
	    toPopulate.append(convertedTime);
	}

	++itt;
    }

    toPopulate.append(trackStart());
    toPopulate.append(trackEnd());
}

QDomDocument DocClipRef::sceneToXML(const GenTime & startTime,
    const GenTime & endTime)
{
    QDomDocument sceneDoc;

    if (m_effectStack.isEmpty()) {
	sceneDoc =
	    m_clip->sceneToXML(startTime - trackStart() + cropStartTime(),
	    endTime - trackStart() + cropStartTime());
    } else {
	// We traverse the effect stack forwards; this let's us add the video clip at the top of the effect stack.
	QDomNode constructedNode =
	    sceneDoc.importNode(m_clip->sceneToXML(startTime -
		trackStart() + cropStartTime(),
		endTime - trackStart() +
		cropStartTime()).documentElement(), true);
	EffectStackIterator itt(m_effectStack);

	while (itt.current()) {
	    QDomElement parElement = sceneDoc.createElement("par");
	    parElement.appendChild(constructedNode);

	    QDomElement effectElement =
		sceneDoc.createElement("videoeffect");
	    effectElement.setAttribute("name",
		(*itt)->effectDescription().name());
	    effectElement.setAttribute("begin", QString::number(0, 'f',
		    10));
	    effectElement.setAttribute("dur",
		QString::number((endTime - startTime).seconds(), 'f', 10));
	    parElement.appendChild(effectElement);

	    constructedNode = parElement;
	    ++itt;
	}
	sceneDoc.appendChild(constructedNode);
    }

    return sceneDoc;
}

void DocClipRef::setSnapMarkers(QValueVector < CommentedTime > markers)
{
    m_clip->setSnapMarkers( markers );
    /*qHeapSort(m_snapMarkers);

    QValueVector < CommentedTime >::Iterator itt = markers.begin();
       while(itt != markers.end()) {
       addSnapMarker((*itt).time(), (*itt).comment(), false);
       ++itt;
       }*/
}

GenTime DocClipRef::adjustTimeToSpeed(GenTime t) const
{
	if (m_speed == 1.0 && m_endspeed == 1.0) return t;
	int pos = (int) (t - m_cropStart).frames(m_clip->framesPerSecond());
	double actual_speed = m_speed + ((double) pos) / (double)((m_trackEnd - m_trackStart).frames(m_clip->framesPerSecond())) * (m_endspeed - m_speed);

	int actual_position = (int) floor((double) pos / actual_speed);
	return GenTime(actual_position, m_clip->framesPerSecond()) + cropStartTime();

	// TODO: markers not adjusted when clip is played reverse
}

QValueVector < GenTime > DocClipRef::snapMarkersOnTrack() const
{
    QValueVector < GenTime > markers;
    QValueVector < CommentedTime > originalMarkers = commentedSnapMarkers();
    markers.reserve(originalMarkers.count());

    for (uint count = 0; count < originalMarkers.count(); ++count) {
	GenTime t = originalMarkers[count].time();
	if (t > m_cropStart && t < m_trackEnd - m_trackStart + m_cropStart) {
	    t = adjustTimeToSpeed(t);
	    if (t < cropStartTime() + cropDuration() && t > cropStartTime()) markers.append(t + trackStart() - cropStartTime());
	}
    }

    return markers;
}

void DocClipRef::addSnapMarker(const GenTime & time, QString comment, bool notify)
{
    m_clip->addSnapMarker(time, comment);
    //if (notify) m_parentTrack->notifyClipChanged(this);
}

void DocClipRef::editSnapMarker(const GenTime & time, QString comment)
{
    m_clip->editSnapMarker(time, comment);
}

QValueVector < GenTime > DocClipRef::transitionSnaps()
{
    QValueVector < GenTime > tranList;
    TransitionStack::iterator itt = m_transitionStack.begin();
    while (itt) {
        tranList.append((*itt)->transitionStartTime());
	tranList.append((*itt)->transitionEndTime());
        ++itt;
    }
    return tranList;
}

void DocClipRef::deleteSnapMarker(const GenTime & time)
{
    if (!m_clip->deleteSnapMarker(time)) //m_parentTrack->notifyClipChanged(this);
    kdError() << "Could not delete marker at time " << time.seconds() << " - it doesn't exist!" << endl;
}

GenTime DocClipRef::hasSnapMarker(const GenTime & time)
{
    return m_clip->hasSnapMarkers(time);
}

void DocClipRef::setCropDuration(const GenTime & time)
{
    setTrackEnd(trackStart() + time);
}

GenTime DocClipRef::trackMiddleTime() const
{
    return (m_trackStart + m_trackEnd) / 2;
}

QValueVector < GenTime > DocClipRef::snapMarkers() const
{
    return m_clip->snapMarkers();
}

QString DocClipRef::markerComment(GenTime t) const
{
    return m_clip->markerComment(t);
}

QValueVector < CommentedTime > DocClipRef::commentedSnapMarkers() const
{
    return m_clip->commentedSnapMarkers();
}

QValueVector < CommentedTime > DocClipRef::commentedTrackSnapMarkers() const
{
    QValueVector < CommentedTime > markers;
    QValueVector < CommentedTime > originalMarkers = commentedSnapMarkers();
    markers.reserve(originalMarkers.count());

    for (uint count = 0; count < originalMarkers.count(); ++count) {
	GenTime t = originalMarkers[count].time();
	if (t > m_cropStart && t < m_trackEnd - m_trackStart + m_cropStart) {
	    t = adjustTimeToSpeed(t);
	    if (t < cropStartTime() + cropDuration() && t > cropStartTime())
	    markers.append(CommentedTime(t + trackStart() - cropStartTime(), originalMarkers[count].comment()));
	}
    }

    return markers;
}

void DocClipRef::addEffect(uint index, Effect * effect)
{
    m_effectStack.insert(index, effect);
}

void DocClipRef::deleteEffect(uint index)
{
    m_effectStack.remove(index);
    if (m_speed != 1.0 || m_endspeed != 1.0) {
	// Check if speed effect was removed. if it was, reset speed to normal
	if (clipEffectNames().findIndex(i18n("Speed")) == -1) {
	    setSpeed(1.0, 1.0);
	}
    }
}


void DocClipRef::setEffectStackSelectedItem(uint ix)
{
    m_effectStack.setSelected(ix);

}

Effect *DocClipRef::selectedEffect()
{
    return m_effectStack.selectedItem();
}

bool DocClipRef::hasEffect()
{
    if (m_effectStack.count() > 0)
	return true;
    return false;
}

void DocClipRef::clearVideoEffects()
{
	for (uint count = 0; count < m_effectStack.count(); ++count)
	{
	    if (m_effectStack.at(count)->effectDescription().type() == "video") {
		    m_effectStack.remove(count);
		    count--;
	    }
	}
}

Effect *DocClipRef::effectAt(uint index) const
{
    EffectStack::iterator itt = m_effectStack.begin();

    if (index < m_effectStack.count()) {
	for (uint count = 0; count < index; ++count)
	    ++itt;
	return *itt;
    }
    //kdError() << "DocClipRef::effectAt() : index out of range" << endl;
    return 0;
}

void DocClipRef::setEffectStack(const EffectStack & effectStack)
{
    m_effectStack = effectStack;
}

const QPixmap & DocClipRef::getAudioImage(int /*width*/, int /*height*/,
	double /*frame*/, double /*numFrames*/, int /*channel*/)
{
    static QPixmap nullPixmap;

    return nullPixmap;
}

void DocClipRef::addTransition(Transition *transition)
{
    m_transitionStack.append(transition);
    if (m_parentTrack) m_parentTrack->notifyClipChanged(this);
}

void DocClipRef::deleteTransition(QDomElement transitionXml)
{
    if (m_transitionStack.isEmpty()) return;
    for ( uint i = 0; i < m_transitionStack.count(); ++i ) {
	Transition *t = m_transitionStack.at(i);
        if ( t && t->toXML().attribute("start") == transitionXml.attribute("start") && t->toXML().attribute("end") == transitionXml.attribute("end")) { 
	m_transitionStack.remove(i);
	}
    }

    if (m_parentTrack) m_parentTrack->notifyClipChanged(this);
}


Transition *DocClipRef::transitionAt(const GenTime &time)
{
    TransitionStack::iterator itt = m_transitionStack.begin();
    while (itt) {
        if ((*itt)->transitionStartTime()<time && (*itt)->transitionEndTime()>time) {
            return (*itt);
	    }
        ++itt;
    }
    return 0;
}

void DocClipRef::deleteTransitions()
{
    m_transitionStack.clear();
    if (m_parentTrack) m_parentTrack->notifyClipChanged(this);
}

bool DocClipRef::hasTransition(DocClipRef *clip)
{
    TransitionStack::iterator itt = m_transitionStack.begin();
    while (itt) {
        if ((*itt)->hasClip(clip)) {
            return true;
        }
        ++itt;
    }
    return false;
}


TransitionStack DocClipRef::clipTransitions()
{
    return m_transitionStack;
}

void DocClipRef::resizeTransitionStart(uint ix, GenTime time)
{
    m_transitionStack.at(ix)->resizeTransitionStart(time);
    if (m_parentTrack) m_parentTrack->notifyClipChanged(this);
}

void DocClipRef::resizeTransitionEnd(uint ix, GenTime time)
{
    m_transitionStack.at(ix)->resizeTransitionEnd(time);
    if (m_parentTrack) m_parentTrack->notifyClipChanged(this);
}

void DocClipRef::moveTransition(uint ix, GenTime time)
{
    m_transitionStack.at(ix)->moveTransition(time);
    if (m_parentTrack) m_parentTrack->notifyClipChanged(this);
}

int DocClipRef::getAudioPart(double from, double length,int channel){
	int ret=0;
	QMap<int,QMap<int,QByteArray> >::Iterator it;
	/** we can have frame fro 7.2 to 10.4
		so we need max(max (7.2-7.99999),
		max(8-10),
		max(10.000-10.2000)
		)
	*/
	/** max from first framepart*/
	size_t startpart=(size_t)(from*(double)AUDIO_FRAME_WIDTH)%AUDIO_FRAME_WIDTH;
	size_t endpart=(size_t)((from+length)*(double)AUDIO_FRAME_WIDTH)%AUDIO_FRAME_WIDTH;
	for (int i=(int)from;i<(int)from+1;i++){
		it=referencedClip()->audioFrameChache.find(i);
		if (it!=referencedClip()->audioFrameChache.end()){
			QMap<int,QByteArray>::iterator it_channel;
			it_channel=(*it).find(channel);
			if (it_channel!=(*it).end()){
				QByteArray arr=*it_channel;
				size_t end=endpart;
				if (length>=1.0)
					end=AUDIO_FRAME_WIDTH-1;
				for (size_t j=startpart;j<=end;j++){
					ret=QMAX(arr[j],ret);
				}
			}
		}
	}

	/** max from full frames between */
	for (int i=(int)from+1;i<from+length /*&& i<(int)from +2*/;i++){
		/*m_thumbcreator->getAudioThumbs(fileURL(),channel,i,
		200.0,10,referencedClip()->audioFrameChache);*/
		it=referencedClip()->audioFrameChache.find(i);
		
		
		if (it!=referencedClip()->audioFrameChache.end()){
			QMap<int,QByteArray>::iterator it_channel;
			it_channel=(*it).find(channel);
			if (it_channel!=(*it).end()){
				QByteArray arr=*it_channel;
				for (int j=0;j<AUDIO_FRAME_WIDTH;j++){
					ret=QMAX(arr[j],ret);
				}
			}
		}
	}
	/** max from last frame part */
	///TODO
	
	return ret;

	
}

QByteArray DocClipRef::getAudioThumbs(int channel, double frame, double frameLength, int arrayWidth){
	
	/** Merge alle Frames into scaled frames*/
	if (frame<0.0)
		frame=0.0;
	QByteArray ret(arrayWidth);
	if (!m_clip->audioThumbCreated) ret.fill(00);
	else for ( int i=0;i< arrayWidth;i++){
		double step=(double)frameLength/(double)arrayWidth;
		double startpos=frame+(double)i*step;
		ret[i]=getAudioPart(startpos,step,channel);
	}
	return ret;

	
}
