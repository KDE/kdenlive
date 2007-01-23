/***************************************************************************
                          doccliptextfile.cpp  -  description
                             -------------------
    begin                : Jan 31 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "docclipvirtual.h"

#include <iostream>
#include <assert.h>

#include <qfileinfo.h>

#include <kdebug.h>
#include <klocale.h>
#include <kio/netaccess.h>

#include "clipmanager.h"
#include "kdenlivesettings.h"


DocClipVirtual::DocClipVirtual(const KURL & url, const QString & name, const QString & text, GenTime start, GenTime end, uint id):DocClipBase(), m_start(start), m_end(end), m_clipType(VIRTUAL), m_duration(end - start), m_durationKnown(true), m_framesPerSecond(KdenliveSettings::defaultfps()), m_url(url)
{
    setName(name);
    setId(id);
    setDescription( text );
    thumbCreator = new KThumb();
    //setThumbnail(NULL);
}


DocClipVirtual::DocClipVirtual(QDomDocument node):DocClipBase(),m_clipType(VIRTUAL), m_duration(0.0),  m_durationKnown(false), m_framesPerSecond(KdenliveSettings::defaultfps())
{
QDomElement element = node.documentElement(); 
 if (element.tagName() != "kdenliveclip") {
	kdWarning() <<
	    "DocClipRef::createClip() element has unknown tagName : " <<
	    element.tagName() << endl;
	return;
    }

    QDomNode n = element.firstChild();

    while (!n.isNull()) {
	QDomElement e = n.toElement();
	if (!e.isNull()) {
	    //kdWarning() << "DocClipRef::createClip() tag = " << e.tagName() << endl;
	    if (e.tagName() == "avfile") {
		kdDebug()<<"++ FOUND CLIP"<<endl;
                setId(e.attribute("id", "-1").toInt());
		m_url = KURL(e.attribute("url", QString::null));
		m_width = e.attribute("width", "0").toInt();
		m_height = e.attribute("height", "0").toInt();
		m_start = GenTime(e.attribute("virtualstart", "0").toInt(), m_framesPerSecond);
		m_end = GenTime(e.attribute("virtualend", "0").toInt(), m_framesPerSecond);
		m_durationKnown = e.attribute("durationknown", "0" ).toInt();
		m_duration = m_end - m_start;
		setName(e.attribute("name", QString::null));
		setDescription(e.attribute("description", QString::null));
	    }
	}
	n = n.nextSibling();
    }
    thumbCreator = new KThumb();

}

DocClipVirtual::~DocClipVirtual()
{
}

void DocClipVirtual::removeTmpFile() const
{
    KIO::NetAccess::del(m_url);
}

const GenTime & DocClipVirtual::duration() const
{
    return m_duration;
}

const DocClipBase::CLIPTYPE & DocClipVirtual::clipType() const
{
    return m_clipType;
}


const KURL & DocClipVirtual::fileURL() const
{
    return m_url;
}


DocClipVirtual *DocClipVirtual::createClip(const QDomElement element)
{
}

bool DocClipVirtual::durationKnown() const
{
    return m_durationKnown;
}

// virtual
double DocClipVirtual::framesPerSecond() const
{
    return m_framesPerSecond;
}

//returns clip video properties -reh
uint DocClipVirtual::clipHeight() const
{
    return KdenliveSettings::defaultheight();
}

uint DocClipVirtual::clipWidth() const
{
    return KdenliveSettings::defaultwidth();
}

// virtual
QDomDocument DocClipVirtual::sceneToXML(const GenTime & startTime,
    const GenTime & endTime) const
{
}

// virtual
QDomDocument DocClipVirtual::generateSceneList(bool, bool) const
{

    QDomDocument sceneList;
    QDomElement westley = sceneList.createElement("westley");
    sceneList.appendChild(westley);

    QDomElement producer = sceneList.createElement("producer");
    producer.setAttribute("id", 0);
    producer.setAttribute("mlt_service", "westley");
    producer.setAttribute("resource", fileURL().path());
    westley.appendChild(producer);
    QDomElement playlist = sceneList.createElement("playlist");
    playlist.setAttribute("in", "0");
    playlist.setAttribute("out", QString::number(duration().frames(m_framesPerSecond)));
    QDomElement entry = sceneList.createElement("entry");
    entry.setAttribute("producer", 0);
    playlist.appendChild(entry);
    westley.appendChild(playlist);

    return sceneList;

}

void DocClipVirtual::populateSceneTimes(QValueVector < GenTime >
    &toPopulate) const
{
    toPopulate.append(GenTime(0));
    toPopulate.append(duration());
}

GenTime DocClipVirtual::virtualStartTime() const
{
    return m_start;
}

GenTime DocClipVirtual::virtualEndTime() const
{
    return m_end;
}

uint DocClipVirtual::fileSize() const
{
    return 0;
}

uint DocClipVirtual::numReferences() const
{
#warning TODO - write this funtion.
}


// virtual
bool DocClipVirtual::referencesClip(DocClipBase * clip) const
{
    return this == clip;
}

// virtual
QDomDocument DocClipVirtual::toXML() const
{
    QDomDocument doc = DocClipBase::toXML();
    QDomNode node = doc.firstChild();

    while (!node.isNull()) {
	QDomElement element = node.toElement();
	if (!element.isNull()) {
	    if (element.tagName() == "kdenliveclip") {
		QDomElement avfile = doc.createElement("avfile");
		avfile.setAttribute("type", m_clipType);
                avfile.setAttribute("id", getId());
		avfile.setAttribute("url", m_url.path());
		avfile.setAttribute("duration", m_duration.frames(m_framesPerSecond));
		avfile.setAttribute("virtualstart", m_start.frames(m_framesPerSecond));
		avfile.setAttribute("virtualend", m_end.frames(m_framesPerSecond));
		avfile.setAttribute("durationknown", m_durationKnown );
		avfile.setAttribute("description", description());
		avfile.setAttribute("name", name());
		avfile.setAttribute("width", m_width);
		avfile.setAttribute("height", m_height);
		element.appendChild(avfile);
		
		return doc;
	    }
	}
	node = node.nextSibling();
    }


    assert(node.isNull());

    /* This final return should never be reached, it is here to remove compiler warning. */
    return doc;
}

// virtual
bool DocClipVirtual::matchesXML(const QDomElement & element) const
{
    bool result = false;

    if (element.tagName() == "kdenliveclip") {
	bool found = false;
	QDomNode n = element.firstChild();
	while (!n.isNull()) {
	    QDomElement avElement = n.toElement();	// try to convert the node to an element.

	    if (!avElement.isNull()) {
		if (avElement.tagName() == "avfile") {
		    if (found) {
			kdWarning() <<
			    "Clip contains multiple avclip definitions, only matching XML of the first one,"
			    << endl;
			break;
		    } else {
			found = true;
			//if(avElement.attribute("url") == fileURL().url()) {
			if (avElement.attribute("id") ==
			    QString::number(getId())) {
			    result = true;
			}
		    }
		}
	    }

	    n = n.nextSibling();
	}
    }

    return result;
}



