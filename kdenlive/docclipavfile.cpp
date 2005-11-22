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
#include <kdebug.h>
#include "clipmanager.h"
#include <qfileinfo.h>


DocClipAVFile::DocClipAVFile(const QString &name, const KURL &url) :
						DocClipBase(),
						m_duration(0.0),
						m_url(url),
						m_durationKnown(false),
						m_framesPerSecond(0)

{
	setName(name);
}

DocClipAVFile::DocClipAVFile(const KURL &url) :
				DocClipBase(),
				m_duration(0.0),
				m_url(url),
				m_durationKnown(false),
				m_framesPerSecond(0)
{
	setName(url.fileName());
}

DocClipAVFile::~DocClipAVFile()
{
}

const GenTime &DocClipAVFile::duration() const
{
	return m_duration;
}

DocClipAVFile::CLIPTYPE DocClipAVFile::clipType() {
  return m_clipType;
}


const KURL &DocClipAVFile::fileURL() const
{
	return m_url;
}

DocClipAVFile * DocClipAVFile::createClip(const QDomElement element)
{
	DocClipAVFile *file = 0;


	if(element.tagName() == "avfile") {
		KURL url(element.attribute("url"));
		file = new DocClipAVFile(url.filename(), url);
	} else {
		kdWarning() << "DocClipAVFile::createClip failed to generate clip" << endl;
	}

	return file;
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
QDomDocument DocClipAVFile::sceneToXML(const GenTime &startTime, const GenTime &endTime) const
{
	QDomDocument sceneList;

	QDomElement property = sceneList.createElement("property");
	property.setAttribute("name", "resource");

	QDomText textNode = sceneList.createTextNode(fileURL().path());

	property.appendChild(textNode);
	
//	property.setAttribute(str_inpoint, QString::number(startTime.seconds()));
//	property.setAttribute(str_outpoint, QString::number(endTime.seconds()));

	sceneList.appendChild(property);

	return sceneList;
}

void DocClipAVFile::populateSceneTimes(QValueVector<GenTime> &toPopulate) const
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
	return 0;
}

// virtual
bool DocClipAVFile::referencesClip(DocClipBase *clip) const
{
	return this == clip;
}

// virtual
QDomDocument DocClipAVFile::toXML() const {
	QDomDocument doc = DocClipBase::toXML();
	QDomNode node = doc.firstChild();

	while( !node.isNull()) {
		QDomElement element = node.toElement();
		if(!element.isNull()) {
			if(element.tagName() == "clip") {
				QDomElement avfile = doc.createElement("avfile");
				avfile.setAttribute("url", fileURL().url());
				element.appendChild(avfile);
				return doc;
			}
		}
		node = node.nextSibling();
	}

	ASSERT(node.isNull());

	/* This final return should never be reached, it is here to remove compiler warning. */
	return doc;
}

// virtual
bool DocClipAVFile::matchesXML(const QDomElement &element) const
{
	bool result = false;

	if(element.tagName() == "clip")
	{
		bool found = false;
		QDomNode n = element.firstChild();
		while( !n.isNull() ) {
        	QDomElement avElement = n.toElement(); // try to convert the node to an element.

			if(!avElement.isNull()) {
				if(avElement.tagName() == "avfile") {
					if(found) {
						kdWarning() << "Clip contains multiple avclip definitions, only matching XML of the first one," << endl;
						break;
					} else {
						found = true;
						if(avElement.attribute("url") == fileURL().url()) {
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
void DocClipAVFile::calculateFileProperties(const QMap<QString, QString> &attributes)
{
	if(m_url.isLocalFile()) {
		QFileInfo fileInfo(m_url.path());

	 	/* Determines the size of the file */
		m_filesize = fileInfo.size();

		if(attributes.contains("duration")) {
			m_duration = GenTime(attributes["duration"].toDouble(), 25);
			m_durationKnown = true;
		} else {
			// No duration known, use an arbitrary one until it is.
	  		m_duration = GenTime(0.0);
			m_durationKnown = false;
		}
		//extend attributes -reh
		if(attributes.contains("type"))
		{
			if (attributes["type"] == "audio") m_clipType = AUDIO;
			else if (attributes["type"] == "video") m_clipType = VIDEO;
			else if (attributes["type"] == "av") m_clipType = AV;
		}else{
			m_clipType = AV;
		}
		if(attributes.contains("height"))
		{
			m_height = attributes["height"].toInt();
		}else{
			m_height = 0;
		}
		if(attributes.contains("width"))
		{
			m_width = attributes["width"].toInt();
		}else{
			m_width = 0;
		}
		//decoder name
		if(attributes.contains("name"))
		{
			m_decompressor = attributes["name"];
		}else{
			m_decompressor = "n/a";
		}
		//video type ntsc/pal
		if(attributes.contains("system"))
		{
			m_system = attributes["system"];
		}else{
			m_system = "n/a";
		}
		if(attributes.contains("fps"))
		{
			m_framesPerSecond = attributes["fps"].toInt();
		} else {
			// No frame rate known.
			m_framesPerSecond = 0;
		}
		//audio attributes -reh
		if(attributes.contains("channels"))
		{
			m_channels = attributes["channels"].toInt();
		} else {
			m_channels = 0;
		}
		if(attributes.contains("format"))
		{
			m_format = attributes["format"];
		} else {
			m_format = "n/a";
		}
		if(attributes.contains("bitspersample"))
		{
			m_bitspersample = attributes["bitspersample"].toInt();
		} else {
			m_bitspersample = 0;
		}

	} else {
		/** If the file is not local, then no file properties are currently returned */
		m_duration = GenTime(0.0);
		m_durationKnown = false;
		m_framesPerSecond = 0;
		m_filesize = 0;
	}
}

