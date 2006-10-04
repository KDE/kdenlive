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
m_trackEnd(0.0), m_parentTrack(0),  m_trackNum(-1), m_clip(clip),  m_speed(1.0)
{
    if (!clip) {
	kdError() <<
	    "Creating a DocClipRef with no clip - not a very clever thing to do!!!"
	    << endl;
    }

    // If clip is a video, resizing it should update the thumbnails
    if (m_clip->clipType() == DocClipBase::VIDEO || m_clip->clipType() == DocClipBase::AV) {
	m_thumbnail = QPixmap();
        m_endthumbnail = QPixmap();
        startTimer = new QTimer( this );
        endTimer = new QTimer( this );
        connect(clip->thumbCreator, SIGNAL(thumbReady(int, QPixmap)),this,SLOT(updateThumbnail(int, QPixmap)));
        connect(this, SIGNAL(getClipThumbnail(KURL, int, int, int)), clip->thumbCreator, SLOT(getImage(KURL, int, int, int)));
        connect( startTimer, SIGNAL(timeout()), this, SLOT(fetchStartThumbnail()));
        connect( endTimer, SIGNAL(timeout()), this, SLOT(fetchEndThumbnail()));
    }
    if (m_clip->clipType() == DocClipBase::AUDIO || m_clip->clipType() == DocClipBase::AV) {
	if (m_clip->clipType() != DocClipBase::AV){
        	m_thumbnail = referencedClip()->thumbnail();
        	m_endthumbnail = m_thumbnail;
	}
	connect(clip->thumbCreator, SIGNAL(audioThumbReady(QMap<int,QMap<int,QByteArray> >)), this, SLOT(updateAudioThumbnail(QMap<int,QMap<int,QByteArray> >)));
	/*connect(this,
		SIGNAL(getAudioThumbnails(KURL, int,double,double,int)), //,QMap<int,QMap<int,QByteArray> >&)),
		clip->thumbCreator,
		SLOT(getAudioThumbs(KURL, int,double,double,int))); //,QMap<int,QMap<int,QByteArray> >&)));
	double lengthInFrames=m_clip->duration().frames(m_clip->framesPerSecond());
	if (KdenliveSettings::audiothumbnails()) emit getAudioThumbnails(fileURL(), 0, 0, lengthInFrames,AUDIO_FRAME_WIDTH);*/ //referencedClip()->audioFrameChache
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
		//double lengthInFrames=m_clip->duration().frames(m_clip->framesPerSecond());
		if (!m_clip->audioThumbCreated) m_clip->toDocClipAVFile()->getAudioThumbs();
	}
	else {
		m_clip->audioThumbCreated = false;
		referencedClip()->audioFrameChache.clear();
		kdDebug()<<"**********  FREED MEM FOR: "<<name()<<", COUNT: "<<referencedClip()->audioFrameChache.count ()<<endl;
	}
}

void DocClipRef::updateAudioThumbnail(QMap<int,QMap<int,QByteArray> >)
{
    if (m_parentTrack) QTimer::singleShot(200, this, SLOT(refreshCurrentTrack()));
//m_parentTrack, SLOT(refreshLayout()));
}

void DocClipRef::refreshCurrentTrack()
{
    m_parentTrack->notifyClipChanged(this);
}

bool DocClipRef::hasVariableThumbnails()
{
    if ((m_clip->clipType() != DocClipBase::VIDEO && m_clip->clipType() != DocClipBase::AV) || !KdenliveSettings::videothumbnails()) 
        return false;
    return true;
}

void DocClipRef::generateThumbnails()
{
    if (m_clip->clipType() == DocClipBase::VIDEO || m_clip->clipType() == DocClipBase::AV) {
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
	bitBlt(&result, 1, 1, &p, 0, 0, width - 2, height - 2);
        m_endthumbnail = result;
        m_thumbnail = result;
    }
    else if (m_clip->clipType() == DocClipBase::TEXT || m_clip->clipType() == DocClipBase::IMAGE) {
	    QPixmap p(fileURL().path());
            QImage im;
            im = p;
            p = im.smoothScale(width - 2, height - 2);
	    bitBlt(&result, 1, 1, &p, 0, 0, width - 2, height - 2);
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
	    bitBlt(&result, 1, 1, &p1, 0, 0, width - 2, height - 2);
    	    m_thumbnail = result;
	    result.fill(Qt::black);
    	    im = p2;
    	    p2 = im.smoothScale(width - 2, height - 2);
	    bitBlt(&result, 1, 1, &p2, 0, 0, width - 2, height - 2);
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
    return m_trackEnd - m_trackStart;
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
    QValueVector < CommentedTime > markers;
    EffectStack effectStack;

    /*kdWarning() << "================================" << endl;
    kdWarning() << "Creating Clip : " << element.ownerDocument().
    toString() << endl;*/

    int trackNum = 0;

    QDomNode node = element;
    node.normalize();

    if (element.tagName() != "clip") {
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
		trackNum = e.attribute("track", "-1").toInt();
		trackStart =
		    GenTime(e.attribute("trackstart", "0").toDouble());
		cropStart =
		    GenTime(e.attribute("cropstart", "0").toDouble());
		cropDuration =
		    GenTime(e.attribute("cropduration", "0").toDouble());
		trackEnd =
		    GenTime(e.attribute("trackend", "-1").toDouble());
		speed = e.attribute("speed", "1.0").toDouble();
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
	    } else if (e.tagName() == "effects") {
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
	clip->setSpeed(speed);
	if (trackEnd.seconds() != -1) {
	    clip->setTrackEnd(trackEnd);
	} else {
            clip->setTrackEnd(trackStart + cropDuration - GenTime(1, clip->framesPerSecond()));
	}
	clip->setParentTrack(0, trackNum);
	clip->setSnapMarkers(markers);
	//clip->setDescription(description);
	clip->setEffectStack(effectStack);
        
        // add Transitions
        if (!t.isNull()) {
            QDomNode transitionNode = t.firstChild();
            while (!transitionNode.isNull()) {
                QDomElement transitionElement = transitionNode.toElement();
                if (!transitionElement.isNull()) {
                    if (transitionElement.tagName() == "transition") {
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

/** Returns the end of the clip on the track. A convenience function, equivalent
to trackStart() + cropDuration() */
GenTime DocClipRef::trackEnd() const
{
    return m_trackEnd;
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
    // #TODO should return the project height
    return 0;
}

uint DocClipRef::clipWidth() const
{
    if (m_clip->isDocClipAVFile())
        return m_clip->toDocClipAVFile()->clipWidth();
    else if (m_clip->isDocClipTextFile())
        return m_clip->toDocClipTextFile()->clipWidth();
    // #TODO should return the project width
    return 0;
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

    if (clip.tagName() != "clip") {
	kdError() <<
	    "Expected tagname of 'clip' in DocClipRef::toXML(), expect things to go wrong!"
	    << endl;
    }

    QDomElement position = doc.createElement("position");
    position.setAttribute("track", QString::number(trackNum()));
    position.setAttribute("trackstart",
	QString::number(trackStart().seconds(), 'f', 10));
    position.setAttribute("cropstart",
	QString::number(cropStartTime().seconds(), 'f', 10));
    position.setAttribute("cropduration",
	QString::number(cropDuration().seconds(), 'f', 10));
    position.setAttribute("trackend", QString::number(trackEnd().seconds(),
	    'f', 10));
    position.setAttribute("speed",
	QString::number(speed(), 'f'));

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

    QDomElement markers = doc.createElement("markers");
    for (uint count = 0; count < m_snapMarkers.count(); ++count) {
	QDomElement marker = doc.createElement("marker");
	marker.setAttribute("time",
	    QString::number(m_snapMarkers[count].time().seconds(), 'f', 10));
	marker.setAttribute("comment", m_snapMarkers[count].comment());
	markers.appendChild(marker);
    }
    clip.appendChild(markers);
    doc.appendChild(clip);

    return doc;
}

QStringList DocClipRef::clipEffectNames()
{
    QStringList effectNames;
    if (!m_effectStack.isEmpty()) {
	EffectStack::iterator itt = m_effectStack.begin();
	while (itt != m_effectStack.end()) {
	    if ((*itt)->isEnabled()) effectNames<<(*itt)->name().upper();
	    ++itt;
	}
    }
    return effectNames;
}

bool DocClipRef::matchesXML(const QDomElement & element) const
{
    bool result = false;
    if (element.tagName() == "clip") {
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
		if (positionElement.attribute("track").toInt() !=
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
    if (m_speed == 1.0) return m_clip->duration();
    else {
	int frameCount = (int)(m_clip->duration().frames(framesPerSecond()) / m_speed);
	return GenTime(frameCount, framesPerSecond());
    }
}

bool DocClipRef::durationKnown() const
{
    return m_clip->durationKnown();
}

double DocClipRef::speed() const
{
    return m_speed;
}

void DocClipRef::setSpeed(double speed)
{
    m_speed = speed;
    if (cropStartTime() + cropDuration() > duration()) 
	setCropDuration(duration() - cropStartTime());
    else if (m_parentTrack) m_parentTrack->notifyClipChanged(this);
}

QDomDocument DocClipRef::generateSceneList()
{
    return m_clip->generateSceneList();
}

QDomDocument DocClipRef::generateXMLTransition(bool hideVideo, bool hideAudio)
{
    QDomDocument transitionList;

    if (!hideVideo && clipType() == DocClipBase::TEXT && m_clip->toDocClipTextFile()->isTransparent()) {
        QDomElement transition = transitionList.createElement("transition");
        transition.setAttribute("in", trackStart().frames(framesPerSecond()));
        transition.setAttribute("out", trackEnd().frames(framesPerSecond()));
        transition.setAttribute("mlt_service", "composite");
        transition.setAttribute("fill", "1");
        //transition.setAttribute("distort", "1");
        //transition.setAttribute("always_active", "1");
        transition.setAttribute("progressive","1");
        transition.setAttribute("a_track", QString::number( playlistNextTrackNum()));
        // Set b_track to the current clip's track index (+1 because we add a black track at pos 0)
        transition.setAttribute("b_track", QString::number(playlistTrackNum()));
        transitionList.appendChild(transition);
    }
    else if (!hideVideo && (clipType() == DocClipBase::IMAGE || clipType() == DocClipBase::SLIDESHOW) && m_clip->toDocClipAVFile()->isTransparent()) {
        QDomElement transition = transitionList.createElement("transition");
        transition.setAttribute("in", trackStart().frames(framesPerSecond()));
        transition.setAttribute("out", trackEnd().frames(framesPerSecond()));
        transition.setAttribute("mlt_service", "composite");
        //transition.setAttribute("always_active", "1");
        transition.setAttribute("fill", "1");
        //transition.setAttribute("distort", "1");
        transition.setAttribute("progressive","1");
        transition.setAttribute("a_track", QString::number(playlistNextTrackNum()));
        // Set b_track to the current clip's track index (+1 because we add a black track at pos 0)
        transition.setAttribute("b_track", QString::number(playlistTrackNum()));
        transitionList.appendChild(transition);
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
            transition.setAttribute("out", QString::number((*itt)->transitionEndTime().frames(framesPerSecond())));
            if ((*itt)->transitionType() == Transition::PIP_TRANSITION) transition.setAttribute("mlt_service", "composite");
            else transition.setAttribute("mlt_service", (*itt)->transitionTag());
            transition.setAttribute("fill", "1");
            //transition.setAttribute("distort", "1");
   
            typedef QMap<QString, QString> ParamMap;
            ParamMap params;
            params = (*itt)->transitionParameters();
            ParamMap::Iterator it;
            for ( it = params.begin(); it != params.end(); ++it ) {
            	transition.setAttribute(it.key(), it.data());
            }

	    if ((*itt)->transitionType() == Transition::LUMA_TRANSITION || (*itt)->transitionType() == Transition::MIX_TRANSITION) {
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
            transitionList.appendChild(transition);
	}
        ++itt;
    }
    return transitionList;
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
    	entry.setAttribute("producer", "producer" + QString::number(m_clip->getId()));
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
			clipFilter.setAttribute("src", KdenliveSettings::currenttmpfolder() + "/" + effectFile );}
//			clipFilter.setAttribute("data", initEffects::ladspaEffectString(ladspaid, params ));
		    	entry.appendChild(clipFilter);

		// end of LADSPA FILTER

	    }
	    else while (effect->parameter(parameterNum)) {
		uint keyFrameNum = 0;
		uint maxValue;
		uint minValue;

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
			 maxValue =(uint)
			effect->effectDescription().
			parameter(parameterNum)->max();
			 minValue =(uint)
			effect->effectDescription().
			parameter(parameterNum)->min();
		    startTag =
			effect->effectDescription().
			parameter(parameterNum)->startTag();
		    endTag =
			effect->effectDescription().
			parameter(parameterNum)->endTag();

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
				    value() * maxValue / 100));
			    clipFilter.setAttribute("out",
				QString::number(in +
				    (effect->parameter(parameterNum)->
					keyframe(count +
					    1)->time()) * duration));
			    clipFilter.setAttribute(endTag,
				QString::number(effect->
				    parameter(parameterNum)->
				    keyframe(count +
					1)->toDoubleKeyFrame()->value() *
				    maxValue / 100));

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
		    else {  //slowmotion effect, use special producer
    				entry.setTagName("producer");
    				entry.setAttribute("mlt_service","framebuffer");
    				entry.setAttribute("id","slowmotion"+ QString::number(m_clip->getId()));
				QString slowmo = fileURL().path() + ":" + QString::number(effect->effectDescription().parameter(0)->value().toDouble() / effect->effectDescription().parameter(0)->factor());
    				entry.setAttribute("resource", slowmo.ascii());
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
    m_snapMarkers = markers;
    /*qHeapSort(m_snapMarkers);

    QValueVector < CommentedTime >::Iterator itt = markers.begin();
       while(itt != markers.end()) {
       addSnapMarker((*itt).time(), (*itt).comment(), false);
       ++itt;
       }*/
}

QValueVector < GenTime > DocClipRef::snapMarkersOnTrack() const
{
    QValueVector < GenTime > markers;
    markers.reserve(m_snapMarkers.count());

    for (uint count = 0; count < m_snapMarkers.count(); ++count) {
	GenTime t = m_snapMarkers[count].time();
	if (t < cropStartTime() + cropDuration() && t > cropStartTime()) markers.append(t + trackStart() - cropStartTime());
    }

    return markers;
}

void DocClipRef::addSnapMarker(const GenTime & time, QString comment, bool notify)
{
    QValueVector < CommentedTime >::Iterator it = m_snapMarkers.begin();
    for ( it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it ) {
	if ((*it).time() >= time)
	    break;
    }


    if ((it != m_snapMarkers.end()) && ((*it).time() == time)) {
	kdError() <<
	    "trying to add Snap Marker that already exists, this will cause inconsistancies with undo/redo"
	    << endl;
    } else {
	CommentedTime t(time, comment);
	m_snapMarkers.insert(it, t);

	if (notify) m_parentTrack->notifyClipChanged(this);
    }

}

void DocClipRef::editSnapMarker(const GenTime & time, QString comment)
{
    QValueVector < CommentedTime >::Iterator it = m_snapMarkers.begin();
    for ( it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it ) {
	if ((*it).time() == time)
	    break;
    }
    if (it != m_snapMarkers.end()) {
	(*it).setComment(comment);
    } else {
	kdError() <<
	    "trying to edit Snap Marker that does not already exists"  << endl;
    }

}

void DocClipRef::deleteSnapMarker(const GenTime & time)
{
    QValueVector < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if ((*itt).time() == time)
	    break;
	++itt;
    }

    if ((itt != m_snapMarkers.end()) && ((*itt).time() == time)) {
	m_snapMarkers.erase(itt);
	m_parentTrack->notifyClipChanged(this);
    } else {
	kdError() << "Could not delete marker at time " << time.
	    seconds() << " - it doesn't exist!" << endl;
    }
}

bool DocClipRef::hasSnapMarker(const GenTime & time)
{
    QValueVector < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if ((*itt).time() == time)
	    return true;
	++itt;
    }

    return false;
}

void DocClipRef::setCropDuration(const GenTime & time)
{
    setTrackEnd(trackStart() + time);
}

GenTime DocClipRef::findPreviousSnapMarker(const GenTime & time)
{
    QValueVector < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if ((*itt).time() >= time)
	    break;
	++itt;
    }

    if (itt != m_snapMarkers.begin()) {
	--itt;
	return (*itt).time();
    } else {
	return GenTime(0);
    }
}

GenTime DocClipRef::findNextSnapMarker(const GenTime & time)
{
    QValueVector < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if (time <= (*itt).time())
	    break;
	++itt;
    }

    if (itt != m_snapMarkers.end()) {
	if ((*itt).time() == time) {
	    ++itt;
	}
	if (itt != m_snapMarkers.end()) {
	    return (*itt).time();
	} else {
	    return GenTime(duration());
	}
    } else {
	return GenTime(duration());
    }
}

GenTime DocClipRef::trackMiddleTime() const
{
    return (m_trackStart + m_trackEnd) / 2;
}

QValueVector < GenTime > DocClipRef::snapMarkers() const
{
    QValueVector < GenTime > markers;
    markers.reserve(m_snapMarkers.count());

    for (uint count = 0; count < m_snapMarkers.count(); ++count) {
	markers.append(m_snapMarkers[count].time());
    }

    return markers;
}

QValueVector < CommentedTime > DocClipRef::commentedSnapMarkers() const
{
    return m_snapMarkers;
}

QValueVector < CommentedTime > DocClipRef::commentedTrackSnapMarkers() const
{
    QValueVector < CommentedTime > markers;
    markers.reserve(m_snapMarkers.count());

    for (uint count = 0; count < m_snapMarkers.count(); ++count) {
	markers.append(CommentedTime(m_snapMarkers[count].time() + trackStart() -
	    cropStartTime(), m_snapMarkers[count].comment()));
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
    /*TransitionStack::iterator itt = m_transitionStack.begin();
    while (itt) {
        if ((*itt)->toXML().attribute("start") == transitionXml.attribute("start") && (*itt)->toXML().attribute("end") == transitionXml.attribute("end")) {
            if (!m_transitionStack.remove(*itt)) kdDebug()<<"*+*+*+*+*+*+* ERROR REMOVE *+*+*"<<endl;
            break;
        }
        ++itt;
    */
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
