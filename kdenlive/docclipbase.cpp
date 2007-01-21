/**************************1*************************************************
                          DocClipBase.cpp  -  description
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

#include <kdebug.h>

#include "docclipbase.h"
#include "docclipavfile.h"
#include "doccliptextfile.h"
#include "docclipproject.h"
#include "doctrackbase.h"
#include "clipmanager.h"

DocClipBase::DocClipBase():
m_description(""), m_refcount(0), m_projectThumbFrame(0), audioThumbCreated(false)
{
    thumbCreator = 0;
}

DocClipBase::~DocClipBase()
{
    delete thumbCreator;
}

void DocClipBase::setName(const QString name)
{
    m_name = name;
}

const QString & DocClipBase::name() const
{
    return m_name;
}

uint DocClipBase::getId() const
{
    return m_id;
}

void DocClipBase::setId( const uint &newId)
{
    m_id = newId;
}

void DocClipBase::setProjectThumbFrame( const uint &ix)
{
    m_projectThumbFrame = ix;
}

uint DocClipBase::getProjectThumbFrame() const
{
    return m_projectThumbFrame;
}

void DocClipBase::setDescription(const QString & description)
{
    m_description = description;
}

const QString & DocClipBase::description() const
{
    return m_description;
}

// virtual
QDomDocument DocClipBase::toXML() const
{
    QDomDocument doc;

    QDomElement clip = doc.createElement("kdenliveclip");
    QDomText text = doc.createTextNode(description());
    clip.appendChild(text);
    doc.appendChild(clip);

    return doc;
}

DocClipBase *DocClipBase::
createClip(const EffectDescriptionList & effectList,
    ClipManager & clipManager, const QDomElement & element)
{
    DocClipBase *clip = 0;
    QString description;
    int trackNum = 0;

    QDomNode node = element;
    node.normalize();

    if (element.tagName() != "kdenliveclip") {
	kdWarning() <<
	    "DocClipBase::createClip() element has unknown tagName : " <<
	    element.tagName() << endl;
	return 0;
    }

    QDomNode n = element.firstChild();

    while (!n.isNull()) {
	QDomElement e = n.toElement();
	if (!e.isNull()) {
	    QString tagName = e.tagName();
	    if (e.tagName() == "avfile") {
		clip = DocClipAVFile::createClip(e);
	    } else if (e.tagName() == "DocTrackBaseList") {
		clip = DocClipProject::createClip(effectList, clipManager, e);
	    } else if (e.tagName() == "position") {
		trackNum = e.attribute("kdenlivetrack", "-1").toInt();
	    }
	} else {
	    QDomText text = n.toText();
	    if (!text.isNull()) {
		description = text.nodeValue();
	    }
	}

	n = n.nextSibling();
    }
    kdWarning() << "DocClipBase::createClip() n is null" << endl;
    if (clip == 0) {
	kdWarning() << "DocClipBase::createClip() unable to create clip" <<
	    endl;
    } else {
	// setup DocClipBase specifics of the clip.
	clip->setDescription(description);
    }
    return clip;
}



QDomDocument DocClipBase::generateSceneList(bool) const
{
}

/*		
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";

	int value = 0;

	QDomDocument sceneList;
	sceneList.appendChild(sceneList.createElement("westley"));

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

	QDomElement producer = sceneList.createElement("playlist");
	producer.setAttribute("id", QString("playlist") + QString::number(value++));
	
	while( (sceneItt != times.end()) && (sceneItt+1 != times.end()) ) {	

		QDomDocument clipDoc = sceneToXML(*sceneItt, *(sceneItt+1));
		QDomElement property;

		if(clipDoc.documentElement().isNull()) {
//			sceneClip = sceneList.createElement("img");
//			sceneClip.setAttribute("rgbcolor", "#000000");
//			sceneClip.setAttribute("dur", QString::number((*(sceneItt+1) - *sceneItt).seconds(), 'f', 10));
//			scene.appendChild(sceneClip);
			property = sceneList.createElement("blank");
			property.setAttribute("length", QString::number((*(sceneItt+1)-*sceneItt).frames(25)));
			//QDomText textNode = sceneList.createTextNode("noise");
			//property.appendChild(textNode);
		} else {
			property = sceneList.importNode(clipDoc.documentElement(), true).toElement();
		}
		producer.appendChild(property);		

		sceneList.documentElement().appendChild(producer);

		++sceneItt;
	}
	kdDebug()<<"+ + + DOCCLIPBASE SCENE: "<<sceneList.toString()<<endl;
	return sceneList;
}
*/

void DocClipBase::setThumbnail(const QPixmap & pixmap)
{
    m_thumbnail = pixmap;
}

const QPixmap & DocClipBase::thumbnail() const
{
    return m_thumbnail;
}

void DocClipBase::updateAudioThumbnail(QMap<int,QMap<int,QByteArray> > data)
{
    audioFrameChache = data;
    audioThumbCreated = true;
}

QValueVector < GenTime > DocClipBase::snapMarkers() const
{
    QValueVector < GenTime > markers;
    markers.reserve(m_snapMarkers.count());

    for (uint count = 0; count < m_snapMarkers.count(); ++count) {
	markers.append(m_snapMarkers[count].time());
    }

    return markers;
}

QValueVector < CommentedTime > DocClipBase::commentedSnapMarkers() const
{
    return m_snapMarkers;
}

void DocClipBase::setSnapMarkers(QValueVector < CommentedTime > markers)
{
    m_snapMarkers = markers;
}

void DocClipBase::addSnapMarker(const GenTime & time, QString comment)
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
    }

}

void DocClipBase::editSnapMarker(const GenTime & time, QString comment)
{
    QValueVector < CommentedTime >::Iterator it;
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

bool DocClipBase::deleteSnapMarker(const GenTime & time)
{
    QValueVector < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if ((*itt).time() == time)
	    break;
	++itt;
    }

    if ((itt != m_snapMarkers.end()) && ((*itt).time() == time)) {
	m_snapMarkers.erase(itt);
	return true;
    } else return false;
}


GenTime DocClipBase::hasSnapMarkers(const GenTime & time)
{
    QValueVector < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if ((*itt).time() == time)
	    return time;
	++itt;
    }

    return GenTime(0.0);
}

GenTime DocClipBase::findPreviousSnapMarker(const GenTime & currTime)
{
    int it;
    for ( it = 0; it < m_snapMarkers.count(); it++ ) {
	if (m_snapMarkers[it].time() >= currTime)
	    break;
    }
    if (it == 0) return GenTime();
    else if (it == m_snapMarkers.count() - 1 && m_snapMarkers[it].time() < currTime)
	return m_snapMarkers[it].time();
    else return m_snapMarkers[it-1].time();
}

GenTime DocClipBase::findNextSnapMarker(const GenTime & currTime)
{
    int it;
    for ( it = 0; it < m_snapMarkers.count(); it++ ) {
	if (m_snapMarkers[it].time() > currTime)
	    break;
    }
    if (it < m_snapMarkers.count() && m_snapMarkers[it].time() > currTime) return m_snapMarkers[it].time();
    return duration();
}

QString DocClipBase::markerComment(GenTime t)
{
    QValueVector < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if ((*itt).time() == t)
	    return (*itt).comment();
	++itt;
    }
    return QString::null;
}