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
		
	m_clipType = AV;
}

DocClipAVFile::DocClipAVFile(const KURL &url) :
				DocClipBase(),
				m_duration(0.0),
				m_url(url),
				m_durationKnown(false),
				m_framesPerSecond(0)
{
	setName(url.fileName());
	m_clipType = AV;
}

DocClipAVFile::~DocClipAVFile()
{
}

GenTime DocClipAVFile::duration() const
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

bool DocClipAVFile::durationKnown()
{
	return m_durationKnown;
}

// virtual
double DocClipAVFile::framesPerSecond() const
{
	return m_framesPerSecond;
}
		
// virtual 
QDomDocument DocClipAVFile::generateSceneList()
{
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";

	QDomDocument scenelist;
	scenelist.appendChild(scenelist.createElement("scenelist"));

	// generate the next scene.
	QDomElement scene = scenelist.createElement("scene");
	scene.setAttribute("duration", QString::number(duration().seconds()));

	QDomElement sceneClip;
	sceneClip = scenelist.createElement("input");
	sceneClip.setAttribute(str_file, fileURL().path());
	sceneClip.setAttribute(str_inpoint, "0.0");
	sceneClip.setAttribute(str_outpoint, QString::number(duration().seconds()));
	scene.appendChild(sceneClip);
	scenelist.documentElement().appendChild(scene);

	return scenelist;
}

// virtual 
QDomDocument DocClipAVFile::sceneToXML(const GenTime &startTime, const GenTime &endTime)
{	
	static QString str_inpoint="inpoint";
	static QString str_outpoint="outpoint";
	static QString str_file="file";		
	QDomDocument sceneList;
	
	QDomElement sceneClip = sceneList.createElement("input");
	sceneClip.setAttribute(str_file, fileURL().path());
	sceneClip.setAttribute(str_inpoint, QString::number(startTime.seconds()));
	sceneClip.setAttribute(str_outpoint, QString::number(endTime.seconds()));

	sceneList.appendChild(sceneClip);

	return sceneList;
}

void DocClipAVFile::populateSceneTimes(QValueVector<GenTime> &toPopulate)
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
QDomDocument DocClipAVFile::toXML() {
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
bool DocClipAVFile::matchesXML(const QDomElement &element)
{
	bool result = false;
	
	if(element.tagName() == "clip")
	{
		QDomNodeList nodeList = element.elementsByTagName("avfile");

		if(nodeList.length() > 0) {
			if(nodeList.length() > 1) {
				kdWarning() << "Clip contains multiple avclip definitions, only matching XML of the first one." << endl;
			}
			QDomElement avElement = nodeList.item(0).toElement();
			if(!avElement.isNull()) {
				if(avElement.attribute("url") == fileURL().url()) {
					result = true;
				}
			}
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
			m_duration = GenTime(attributes["duration"].toDouble());
			m_durationKnown = true;      
		} else {
			// No duration known, use an arbitrary one until it is.
	  		m_duration = GenTime(0.0);
			m_durationKnown = false;
		}

		if(attributes.contains("fps"))
		{
			m_framesPerSecond = attributes["fps"].toInt();
		} else {
			// No frame rate known.
			m_framesPerSecond = 0;
		}

	} else {
		/** If the file is not local, then no file properties are currently returned */
		m_duration = GenTime(0.0);
		m_durationKnown = false;
		m_framesPerSecond = 0;
		m_filesize = 0;
	}
}

