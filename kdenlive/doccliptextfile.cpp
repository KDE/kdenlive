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

#include "doccliptextfile.h"

#include <iostream>
#include <assert.h>

#include <qfileinfo.h>

#include <kdebug.h>
#include <klocale.h>


#include "clipmanager.h"



DocClipTextFile::DocClipTextFile(const QString & name, const KURL & url,
    uint id):DocClipBase(), m_duration(0.0), m_url(url),
m_durationKnown(false), m_framesPerSecond(0),
m_clipType(TEXT), m_alphaTransparency(false)
{
    setName(name);
    setId(id);
}


DocClipTextFile::DocClipTextFile(const QString & name, const QString & text,
                                 const GenTime & duration, const QDomDocument &xml, KURL url, const QPixmap &pix, bool transparency, uint id):DocClipBase(), m_duration(duration), m_xml(xml),
m_url(url), m_durationKnown(true), m_framesPerSecond(25), m_clipType(TEXT), m_alphaTransparency(transparency), m_filesize(0)
{
    setName(name);
    setId(id);
    setDescription( text );
    setThumbnail(pix);
}


DocClipTextFile::DocClipTextFile(const KURL & url):DocClipBase(),
m_duration(0.0),
m_url(url),
m_durationKnown(false),
m_framesPerSecond(0), m_clipType(TEXT), m_alphaTransparency(false)
{
    setName(url.fileName());
}

DocClipTextFile::~DocClipTextFile()
{
}

const GenTime & DocClipTextFile::duration() const
{
    return m_duration;
}

const DocClipBase::CLIPTYPE & DocClipTextFile::clipType() const
{
    return m_clipType;
}


const KURL & DocClipTextFile::fileURL() const
{
    return m_url;
}

const QDomDocument & DocClipTextFile::textClipXml() const
{
    return m_xml;
}

void DocClipTextFile::setTextClipXml(const QDomDocument &xml)
{
    m_xml = xml;
}

void DocClipTextFile::setAlpha(bool transp)
{
    m_alphaTransparency = transp;
}

bool DocClipTextFile::isTransparent()
{
    return m_alphaTransparency;
}

DocClipTextFile *DocClipTextFile::createClip(const QDomElement element)
{
    /*
	DocClipTextFile *file = 0;


	if(element.tagName() == "avfile") {
		KURL url(element.attribute("url"));
		file = new DocClipTextFile(url.filename(), url);
	} else {
		kdWarning() << "DocClipTextFile::createClip failed to generate clip" << endl;
	}

	return file;
    */
}

void DocClipTextFile::setFileURL(const KURL & url)
{
    m_url = url;
    QFileInfo fileInfo(m_url.path());
    /* Determines the size of the file */
    m_filesize = fileInfo.size();
}

void DocClipTextFile::setDuration(const GenTime & duration)
{
    m_durationKnown = true;
    m_duration = duration;
}


bool DocClipTextFile::durationKnown() const
{
    return m_durationKnown;
}

// virtual
double DocClipTextFile::framesPerSecond() const
{
    return m_framesPerSecond;
}

//returns clip video properties -reh
uint DocClipTextFile::clipHeight() const
{
    return 576;
}

uint DocClipTextFile::clipWidth() const
{
    return 720;
}


// virtual
QDomDocument DocClipTextFile::sceneToXML(const GenTime & startTime,
    const GenTime & endTime) const
{
}

// virtual
QDomDocument DocClipTextFile::generateSceneList() const
{
    QDomDocument sceneList;
    QDomElement westley = sceneList.createElement("westley");
    sceneList.appendChild(westley);

    QDomElement producer = sceneList.createElement("producer");
    producer.setAttribute("id", QString("producer0"));
    producer.setAttribute("mlt_service", "pixbuf");
    producer.setAttribute("resource", fileURL().path());
    westley.appendChild(producer);
    QDomElement playlist = sceneList.createElement("playlist");
    playlist.setAttribute("in", "0");
    playlist.setAttribute("out",
                          QString::number(duration().frames(25)));
    QDomElement entry = sceneList.createElement("entry");
    entry.setAttribute("producer", QString("producer0"));
    playlist.appendChild(entry);
    westley.appendChild(playlist);
    /*

	QDomElement westley = sceneList.createElement("westley");
	sceneList.appendChild(westley);

	QDomElement producer = sceneList.createElement("producer");
	producer.setAttribute("id", QString("producer0"));
	producer.setAttribute("mlt_service", "pango");
        producer.setAttribute("text", description());
        producer.setAttribute("text", description());
        producer.setAttribute("bgcolour", "0x000000ff");
	westley.appendChild(producer);
	QDomElement playlist = sceneList.createElement("playlist");
	playlist.setAttribute("in", "0");
	playlist.setAttribute("out",
	    QString::number(duration().frames(25)));
	QDomElement entry = sceneList.createElement("entry");
	entry.setAttribute("producer", QString("producer0"));
	playlist.appendChild(entry);
    westley.appendChild(playlist);*/

        kdDebug()<<"----------------------------------------------"<<endl;
        kdDebug()<<sceneList.toString()<<endl;
        kdDebug()<<"----------------------------------------------"<<endl;
    return sceneList;
}

void DocClipTextFile::populateSceneTimes(QValueVector < GenTime >
    &toPopulate) const
{
    toPopulate.append(GenTime(0));
    toPopulate.append(duration());
}

uint DocClipTextFile::fileSize() const
{
    return m_filesize;
}

uint DocClipTextFile::numReferences() const
{
#warning TODO - write this funtion.
}


// virtual
bool DocClipTextFile::referencesClip(DocClipBase * clip) const
{
    return this == clip;
}

// virtual
QDomDocument DocClipTextFile::toXML() const
{

    /*QDomDocument doc;

       QDomElement clip = doc.createElement("clip");
       clip.setAttribute("name", name());
       QDomText text = doc.createTextNode(description());
       clip.appendChild(text);

       QDomElement clip2 = doc.createElement("clip");
       clip2.setAttribute("name", name());
       QDomText text2 = doc.createTextNode(description());
       clip2.appendChild(text2);

       doc.appendChild(clip);
       doc.appendChild(clip2);
     */


    QDomDocument doc = DocClipBase::toXML();
    QDomNode node = doc.firstChild();

    while (!node.isNull()) {
	QDomElement element = node.toElement();
	if (!element.isNull()) {
	    if (element.tagName() == "clip") {
		QDomElement avfile = doc.createElement("avfile");
		//avfile.setAttribute("url", fileURL().url());
		avfile.setAttribute("type", m_clipType);
                avfile.setAttribute("id", getId());
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
bool DocClipTextFile::matchesXML(const QDomElement & element) const
{
    bool result = false;

    if (element.tagName() == "clip") {
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



