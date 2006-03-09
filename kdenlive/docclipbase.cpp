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
m_description(""), m_refcount(0)
{
}

DocClipBase::~DocClipBase()
{
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

    QDomElement clip = doc.createElement("clip");
    clip.setAttribute("name", name());

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

    if (element.tagName() != "clip") {
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
	    } else if (e.tagName() == "project") {
		clip =
		    DocClipProject::createClip(effectList, clipManager, e);
	    } else if (e.tagName() == "position") {
		trackNum = e.attribute("track", "-1").toInt();
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



QDomDocument DocClipBase::generateSceneList() const
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
