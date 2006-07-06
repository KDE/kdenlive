/***************************************************************************
                          DocClipAVFile.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 2002
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

#include "docclipavfile.h"

#include <iostream>
#include <assert.h>

#include <qfileinfo.h>

#include <kdebug.h>
#include <klocale.h>
#include <kio/netaccess.h>


#include "clipmanager.h"
#include "kdenlivesettings.h"

#define AUDIO_FRAME_WIDTH 20


DocClipAVFile::DocClipAVFile(const QString & name, const KURL & url,
    uint id):DocClipBase(), m_duration(0.0), m_url(url),
m_durationKnown(false), m_framesPerSecond(0), m_color(QString::null),
m_clipType(NONE), m_alphaTransparency(false)
{
    setName(name);
    setId(id);
}

/* color clip */
DocClipAVFile::DocClipAVFile(const QString & color,
    const GenTime & duration, uint id):DocClipBase(), m_duration(duration),
m_url(QString::null), m_durationKnown(true), m_framesPerSecond(25),
m_color(color), m_clipType(COLOR), m_filesize(0), m_alphaTransparency(false)
{
    setName(i18n("Color Clip"));
    m_width = KdenliveSettings::defaultwidth();
    m_height = KdenliveSettings::defaultheight();
    setId(id);
}

/*  Image clip */
DocClipAVFile::DocClipAVFile(const KURL & url, const QString & extension,
    const int &ttl, const GenTime & duration, bool alphaTransparency, uint id):DocClipBase(),
m_duration(duration), m_url(url), m_durationKnown(true),
m_framesPerSecond(25), m_color(QString::null), m_clipType(IMAGE), m_alphaTransparency(alphaTransparency)
{
    setName(url.fileName());
    
    setId(id);
    QFileInfo fileInfo(m_url.path());
    QPixmap p(m_url.path());
    m_width = p.width();
    m_height = p.height();
    /* Determines the size of the file */
    m_filesize = fileInfo.size();
    
}

DocClipAVFile::DocClipAVFile(const KURL & url):DocClipBase(),
m_duration(0.0),
m_url(url),
m_durationKnown(false),
m_framesPerSecond(0), m_color(QString::null), m_clipType(NONE), m_alphaTransparency(false)
{
    setName(url.fileName());
    // #TODO: What about the id of these clips ?
}

DocClipAVFile::~DocClipAVFile()
{
}

bool DocClipAVFile::isTransparent()
{
    return m_alphaTransparency;
}

void DocClipAVFile::setAlpha(bool transp)
{
    m_alphaTransparency = transp;
}

const GenTime & DocClipAVFile::duration() const
{
    return m_duration;
}

const DocClipBase::CLIPTYPE & DocClipAVFile::clipType() const
{
    return m_clipType;
}


const KURL & DocClipAVFile::fileURL() const
{
    return m_url;
}

DocClipAVFile *DocClipAVFile::createClip(const QDomElement element)
{
	DocClipAVFile *file = 0;
	if(element.tagName() == "avfile") {
		KURL url(element.attribute("url"));
    		if (KIO::NetAccess::exists(url, true, 0))
			file = new DocClipAVFile(url);
	} else {
		kdWarning() << "DocClipAVFile::createClip failed to generate clip" << endl;
	}

	return file;
}

void DocClipAVFile::setFileURL(const KURL & url)
{
    m_url = url;
    setName(url.fileName());
    QFileInfo fileInfo(m_url.path());
    m_durationKnown = false;
    /* Determines the size of the file */
    m_filesize = fileInfo.size();
}

void DocClipAVFile::setDuration(const GenTime & duration)
{
    m_durationKnown = true;
    m_duration = duration;
}

void DocClipAVFile::setColor(const QString color)
{
    m_color = color;
}

const QString & DocClipAVFile::color() const
{
    return m_color;
}

bool DocClipAVFile::durationKnown() const
{
    return m_durationKnown;
}

// virtual
double DocClipAVFile::framesPerSecond() const
{
    return m_framesPerSecond;
}

//returns clip video properties -reh
uint DocClipAVFile::clipHeight() const
{
    return m_height;
}

uint DocClipAVFile::clipWidth() const
{
    return m_width;
}

QString DocClipAVFile::avDecompressor()
{
    return m_decompressor;
}

QString DocClipAVFile::avSystem()
{
    return m_system;
}

//returns clip audio properties -reh
uint DocClipAVFile::audioChannels() const
{
    return m_channels;
}

QString DocClipAVFile::audioFormat()
{
    return m_format;
}

uint DocClipAVFile::audioBits() const
{
    return m_bitspersample;
}

uint DocClipAVFile::audioFrequency() const
{
    return m_frequency;
}

void DocClipAVFile::getAudioThumbs()
{
    double lengthInFrames=duration().frames(framesPerSecond());
    thumbCreator->getAudioThumbs(fileURL(), 0, 0, lengthInFrames,AUDIO_FRAME_WIDTH);
}

/*
// virtual
QDomDocument DocClipAVFile::generateSceneList() const
{
	static QString str_inpoint="clipBegin";
	static QString str_outpoint="clipEnd";
	static QString str_file="src";

	QDomDocument scenelist;
	scenelist.appendChild(scenelist.createElement("scenelist"));

	// generate the next scene.
	QDomElement scene = scenelist.createElement("seq");

	QDomElement sceneClip;
	sceneClip = scenelist.createElement("video");
	sceneClip.setAttribute(str_file, fileURL().path());
	sceneClip.setAttribute(str_inpoint, "0.0");
	sceneClip.setAttribute(str_outpoint, QString::number(duration().seconds()));
	scene.appendChild(sceneClip);
	scenelist.documentElement().appendChild(scene);

	return scenelist;
}
*/

// virtual
QDomDocument DocClipAVFile::sceneToXML(const GenTime & startTime,
    const GenTime & endTime) const
{
}

// virtual
QDomDocument DocClipAVFile::generateSceneList() const
{
    QDomDocument sceneList;

    if (clipType() == IMAGE) {
	QDomElement westley = sceneList.createElement("westley");
	sceneList.appendChild(westley);
        
        
       	QDomElement producer = sceneList.createElement("producer");
        producer.setAttribute("id", QString("producer0"));
        producer.setAttribute("resource", fileURL().path());
        double ratio = ((double) KdenliveSettings::defaultwidth()/KdenliveSettings::defaultheight())/((double)clipWidth()/clipHeight()) * KdenliveSettings::aspectratio();
        producer.setAttribute("aspect_ratio", QString::number(ratio));
        westley.appendChild(producer);
        QDomElement playlist = sceneList.createElement("playlist");
        playlist.setAttribute("in", "0");
        playlist.setAttribute("out",
        QString::number(duration().frames(25)));
        QDomElement entry = sceneList.createElement("entry");
        entry.setAttribute("producer", QString("producer0"));
        playlist.appendChild(entry);
        westley.appendChild(playlist);
    }

    else if (clipType() == COLOR) {
	QDomElement westley = sceneList.createElement("westley");
	sceneList.appendChild(westley);

	QDomElement producer = sceneList.createElement("producer");
	producer.setAttribute("id", QString("producer0"));
	producer.setAttribute("mlt_service", "colour");
	producer.setAttribute("colour", color());
	westley.appendChild(producer);
	QDomElement playlist = sceneList.createElement("playlist");
	playlist.setAttribute("in", "0");
	playlist.setAttribute("out",
	    QString::number(duration().frames(25)));
	QDomElement entry = sceneList.createElement("entry");
	entry.setAttribute("producer", QString("producer0"));
	playlist.appendChild(entry);
	westley.appendChild(playlist);
    }


    else {
	QDomElement producer = sceneList.createElement("producer");
	producer.setAttribute("id",
	    QString("producer") + QString::number(getId()));
	producer.setAttribute("resource", fileURL().path());
	sceneList.appendChild(producer);
    }

    //kdDebug()<<"START AV: "<<startTime.frames(25)<<endl;
    //kdDebug()<<"STOP AV: "<<endTime.frames(25)<<endl;
    //producer.setAttribute("in", QString::number(startTime.frames(25)));
    //producer.setAttribute("out", QString::number(endTime.frames(25)));



//      kdDebug()<<"++++++++++++CLIP SCENE:\n"<<sceneList.toString()<<endl;


    return sceneList;
}

void DocClipAVFile::populateSceneTimes(QValueVector < GenTime >
    &toPopulate) const
{
    toPopulate.append(GenTime(0));
    toPopulate.append(duration());
}

uint DocClipAVFile::fileSize() const
{
    return m_filesize;
}

uint DocClipAVFile::numReferences() const
{
#warning TODO - write this funtion.
}


// virtual
bool DocClipAVFile::referencesClip(DocClipBase * clip) const
{
    return this == clip;
}

// virtual
QDomDocument DocClipAVFile::toXML() const
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
		avfile.setAttribute("url", fileURL().url());
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
bool DocClipAVFile::matchesXML(const QDomElement & element) const
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


/** Calculates properties for this file that will be useful for the rest of the program. */
void DocClipAVFile::calculateFileProperties(const QMap < QString,
    QString > &attributes)
{
    if (m_url.isLocalFile()) {
	QFileInfo fileInfo(m_url.path());

	/* Determines the size of the file */
	m_filesize = fileInfo.size();

	if (attributes.contains("duration")) {
	    m_duration = GenTime(attributes["duration"].toInt(), 25.0);
	    m_durationKnown = true;
	} else {
	    // No duration known, use an arbitrary one until it is.
	    m_duration = GenTime(0.0);
	    m_durationKnown = false;
	}
	//extend attributes -reh
	if (attributes.contains("type")) {
	    if (attributes["type"] == "audio")
		m_clipType = AUDIO;
	    else if (attributes["type"] == "video")
		m_clipType = VIDEO;
	    else if (attributes["type"] == "av")
		m_clipType = AV;
	} else {
	    m_clipType = AV;
	}
	if (attributes.contains("height")) {
	    m_height = attributes["height"].toInt();
	} else {
	    m_height = 0;
	}
	if (attributes.contains("width")) {
	    m_width = attributes["width"].toInt();
	} else {
	    m_width = 0;
	}
	//decoder name
	if (attributes.contains("name")) {
	    m_decompressor = attributes["name"];
	} else {
	    m_decompressor = "n/a";
	}
	//video type ntsc/pal
	if (attributes.contains("system")) {
	    m_system = attributes["system"];
	} else {
	    m_system = "n/a";
	}
	if (attributes.contains("fps")) {
	    m_framesPerSecond = attributes["fps"].toInt();
	} else {
	    // No frame rate known.
	    m_framesPerSecond = 0;
	}
	//audio attributes -reh
	if (attributes.contains("channels")) {
	    m_channels = attributes["channels"].toInt();
	} else {
	    m_channels = 0;
	}
	if (attributes.contains("format")) {
	    m_format = attributes["format"];
	} else {
	    m_format = "n/a";
	}
	if (attributes.contains("frequency")) {
	    m_frequency = attributes["frequency"].toInt();
	} else {
	    m_frequency = 0;
	}

    } else {
		/** If the file is not local, then no file properties are currently returned */
	m_duration = GenTime(0.0);
	m_durationKnown = false;
	m_framesPerSecond = 0;
	m_filesize = 0;
    }
}
