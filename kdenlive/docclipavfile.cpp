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
m_clipType(NONE), m_alphaTransparency(false), m_loop(false), m_channels(0), m_frequency(0), m_ttl(0), m_hasCrossfade(false), m_audioTimer(0)
{
    thumbCreator = new KThumb();
    m_audioTimer = new QTimer( this );
    connect(m_audioTimer, SIGNAL(timeout()), this, SLOT(getAudioThumbs()));
    setName(name);
    setId(id);
}

/* color clip */
DocClipAVFile::DocClipAVFile(const QString & color, const GenTime & duration, uint id):DocClipBase(),  m_duration(duration),
m_url(QString::null), m_hasCrossfade(false), m_durationKnown(true),m_framesPerSecond(KdenliveSettings::defaultfps()),
m_color(color), m_clipType(COLOR), m_filesize(0), m_alphaTransparency(false), m_loop(false), m_channels(0),m_frequency(0), m_ttl(0), m_videoCodec(NULL), m_audioCodec(NULL), m_audioTimer(0)
{
    setName(i18n("Color Clip"));
    m_width = KdenliveSettings::defaultwidth();
    m_height = KdenliveSettings::defaultheight();
    setId(id);
}

/*  Image clip */
DocClipAVFile::DocClipAVFile(const KURL & url, const GenTime & duration, bool alphaTransparency, uint id):DocClipBase(),
m_duration(duration), m_hasCrossfade(false), m_url(url), m_durationKnown(true),
m_framesPerSecond(KdenliveSettings::defaultfps()), m_color(QString::null), m_clipType(IMAGE), m_alphaTransparency(alphaTransparency), m_loop(false), m_channels(0), m_frequency(0), m_ttl(0), m_videoCodec(NULL), m_audioCodec(NULL), m_audioTimer(0)
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

/*  Slideshow clip */
DocClipAVFile::DocClipAVFile(const KURL & url, const QString & extension,
    const int &ttl, const GenTime & duration, bool alphaTransparency, bool loop, bool crossfade, const QString &lumaFile, double lumasoftness, uint lumaduration, uint id):DocClipBase(),
m_duration(duration), m_url(url), m_durationKnown(true), m_hasCrossfade(crossfade), m_luma(lumaFile), m_lumasoftness(lumasoftness), m_lumaduration(lumaduration),
m_framesPerSecond(KdenliveSettings::defaultfps()), m_color(QString::null), m_clipType(SLIDESHOW), m_alphaTransparency(alphaTransparency), m_loop(loop), m_channels(0), m_frequency(0), m_ttl(ttl), m_videoCodec(NULL), m_audioCodec(NULL), m_audioTimer(0)
{
    setName(i18n("Slideshow"));
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
m_url(url), m_hasCrossfade(false), 
m_durationKnown(false), 
m_framesPerSecond(0), m_color(QString::null), m_clipType(NONE), m_alphaTransparency(false), m_loop(false), m_channels(0), m_frequency(0),  m_ttl(0), m_videoCodec(NULL), m_audioCodec(NULL), m_audioTimer(0)
{
    setName(url.fileName());
    thumbCreator = new KThumb();
    m_audioTimer = new QTimer( this );
    connect(m_audioTimer, SIGNAL(timeout()), this, SLOT(getAudioThumbs()));
    // #TODO: What about the id of these clips ?
}

DocClipAVFile::DocClipAVFile(QDomDocument node):DocClipBase()
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
		m_url = KURL(e.attribute("url", QString::null));
		uint clipType = e.attribute("type", QString::null).toInt();
                setId(e.attribute("id", "-1").toInt());
                m_filesize = e.attribute("filesize", "0").toInt();
		m_alphaTransparency = e.attribute("transparency", "0").toInt();
		if (clipType == DocClipBase::SLIDESHOW) {
		    m_hasCrossfade = e.attribute("crossfade", "0").toInt();
		    m_loop = e.attribute("loop", "0").toInt();
		    m_ttl = e.attribute("ttl", "0").toInt();
		    m_luma = e.attribute("lumafile", QString::null);
		    m_lumasoftness = e.attribute("lumasoftness", "0").toDouble();
		    m_lumaduration = e.attribute("lumaduration", "0").toInt();
		}
		m_width = e.attribute("width", "0").toInt();
		m_height = e.attribute("height", "0").toInt();
		m_channels = e.attribute("channels", "0").toInt();
		m_frequency = e.attribute("frequency", "0").toInt();
		m_videoCodec = e.attribute("videocodec", QString::null);
		m_audioCodec = e.attribute("audiocodec", QString::null);
		m_durationKnown = e.attribute("durationknown", "0" ).toInt();
		if (clipType == DocClipBase::COLOR) m_color = e.attribute("color", QString::null);
		m_duration = GenTime(e.attribute("duration", "0").toInt(), KdenliveSettings::defaultfps());
		setName(e.attribute("name", QString::null));
		setDescription(e.attribute("description", QString::null));
		m_audioTimer = 0;
		switch (clipType) {
		    case DocClipBase::NONE:
			thumbCreator = new KThumb();
			m_audioTimer = new QTimer( this );
			connect(m_audioTimer, SIGNAL(timeout()), this, SLOT(getAudioThumbs()));
			m_clipType = DocClipBase::NONE;
			break;
		    case DocClipBase::AUDIO:
			thumbCreator = new KThumb();
			m_audioTimer = new QTimer( this );
			connect(m_audioTimer, SIGNAL(timeout()), this, SLOT(getAudioThumbs()));
			m_clipType = DocClipBase::AUDIO;
			break;
		    case DocClipBase::VIDEO:
			thumbCreator = new KThumb();
			m_clipType = DocClipBase::VIDEO;
			break;
		    case DocClipBase::PLAYLIST:
			thumbCreator = new KThumb();
			m_audioTimer = new QTimer( this );
			connect(m_audioTimer, SIGNAL(timeout()), this, SLOT(getAudioThumbs()));
			m_clipType = DocClipBase::PLAYLIST;
			break;
		    case DocClipBase::AV:
			thumbCreator = new KThumb();
			m_audioTimer = new QTimer( this );
			connect(m_audioTimer, SIGNAL(timeout()), this, SLOT(getAudioThumbs()));
			m_clipType = DocClipBase::AV;
			break;
		    case DocClipBase::COLOR:
			m_clipType = DocClipBase::COLOR;
			break;
		    case DocClipBase::IMAGE:
			m_clipType = DocClipBase::IMAGE;
			break;
		    case DocClipBase::TEXT:
			m_clipType = DocClipBase::TEXT;
			break;
		    case DocClipBase::SLIDESHOW:
			m_clipType = DocClipBase::SLIDESHOW;
			break;
		}
	    }
	}
	n = n.nextSibling();
    }

}


DocClipAVFile::~DocClipAVFile()
{
    if (m_audioTimer) delete m_audioTimer;
}

void DocClipAVFile::removeTmpFile() const
{
    if (thumbCreator) thumbCreator->removeAudioThumb();
}

void DocClipAVFile::prepareThumbs() const
{
    if (m_audioTimer) m_audioTimer->start(1000, true);
}

bool DocClipAVFile::isTransparent() const
{
    return m_alphaTransparency;
}

bool DocClipAVFile::loop() const
{
    return m_loop;
}

void DocClipAVFile::setLoop(bool isOn)
{
    m_loop = isOn;
}

bool DocClipAVFile::hasCrossfade() const
{
    return m_hasCrossfade;
}

void DocClipAVFile::setCrossfade( bool cross)
{
    m_hasCrossfade = cross;
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

//returns slideshow time for each image (in frames)
int DocClipAVFile::clipTtl() const
{
    return m_ttl;
}

void DocClipAVFile::setClipTtl(const int &ttl)
{
    m_ttl = ttl;
}

void DocClipAVFile::setLumaFile(const QString & luma)
{
    m_luma = luma;
}

const QString & DocClipAVFile::lumaFile() const
{
    return m_luma;
}

void DocClipAVFile::setLumaSoftness(const double & softness)
{
    m_lumasoftness = softness;
}

const double & DocClipAVFile::lumaSoftness() const
{
    return m_lumasoftness;
}

void DocClipAVFile::setLumaDuration(const uint & duration)
{
    m_lumaduration = duration;
}

const uint & DocClipAVFile::lumaDuration() const
{
    return m_lumaduration;
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
    thumbCreator->getAudioThumbs(fileURL(), m_channels, 0, lengthInFrames,AUDIO_FRAME_WIDTH);
}

void DocClipAVFile::stopAudioThumbs()
{
    thumbCreator->stopAudioThumbs();
}

double DocClipAVFile::aspectRatio() const
{
    return KdenliveSettings::aspectratio();
    double ratio = ((double) KdenliveSettings::defaultwidth()/KdenliveSettings::defaultheight())/((double)clipWidth()/clipHeight()) * KdenliveSettings::aspectratio();
    return ratio;
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
QDomDocument DocClipAVFile::generateSceneList(bool, bool) const
{
    QDomDocument sceneList;
    QDomElement westley = sceneList.createElement("westley");
    sceneList.appendChild(westley);
    QDomElement playlist = sceneList.createElement("playlist");

    if (clipType() == IMAGE || clipType() == SLIDESHOW ) {

       	QDomElement producer = sceneList.createElement("producer");
        producer.setAttribute("id", 0);
        producer.setAttribute("resource", fileURL().path());
        if (KdenliveSettings::distortimages()) producer.setAttribute("aspect_ratio", QString::number(aspectRatio()));
	// else producer.setAttribute("aspect_ratio", QString::number(KdenliveSettings::aspectratio()));
	
        westley.appendChild(producer);
        playlist.setAttribute("in", "0");
        playlist.setAttribute("out",
        QString::number(duration().frames(KdenliveSettings::defaultfps())));
        QDomElement entry = sceneList.createElement("entry");
        entry.setAttribute("producer", 0);

	if (clipType() == SLIDESHOW) {
	    producer.setAttribute("ttl", clipTtl());
    	    if (hasCrossfade()) {
    		QDomElement clipFilter =
		sceneList.createElement("filter");
        	clipFilter.setAttribute("mlt_service", "luma");
		clipFilter.setAttribute("period", QString::number(clipTtl()));
		clipFilter.setAttribute("resource", lumaFile());
		clipFilter.setAttribute("luma.softness", QString::number(lumaSoftness()));
		//clipFilter.setAttribute("luma.out", QString::number(lumaDuration()));
		entry.appendChild(clipFilter);
    		}
	}
        playlist.appendChild(entry);
        westley.appendChild(playlist);
    }

    else if (clipType() == COLOR) {
	QDomElement producer = sceneList.createElement("producer");
	producer.setAttribute("id", getId());
	producer.setAttribute("mlt_service", "colour");
	producer.setAttribute("colour", color());
	westley.appendChild(producer);
	QDomElement entry = sceneList.createElement("entry");
	entry.setAttribute("producer", getId());
	entry.setAttribute("in", "0");
	entry.setAttribute("out",
	    QString::number(duration().frames(KdenliveSettings::defaultfps())));
	playlist.appendChild(entry);
	//playlist.appendChild(producer);
	westley.appendChild(playlist);
    }


    else {
	QDomElement producer = sceneList.createElement("producer");
	producer.setAttribute("id", getId());
	producer.setAttribute("resource", fileURL().path());
	playlist.appendChild(producer);
	westley.appendChild(playlist);
    }

    //kdDebug()<<"++++++++++++CLIP SCENE:\n"<<sceneList.toString()<<endl;


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

QString DocClipAVFile::videoCodec() const
{
    return m_videoCodec;
}

QString DocClipAVFile::audioCodec() const
{
    return m_audioCodec;
}

// virtual
bool DocClipAVFile::referencesClip(DocClipBase * clip) const
{
    return this == clip;
}

// virtual
QDomDocument DocClipAVFile::toXML() const
{

    QDomDocument doc = DocClipBase::toXML();
    QDomNode node = doc.firstChild();

    while (!node.isNull()) {
	QDomElement element = node.toElement();
	if (!element.isNull()) {
	    if (element.tagName() == "kdenliveclip") {
		QDomElement avfile = doc.createElement("avfile");
		avfile.setAttribute("url", fileURL().url());
		avfile.setAttribute("type", m_clipType);
                avfile.setAttribute("id", getId());
		avfile.setAttribute("durationknown", m_durationKnown );
		avfile.setAttribute("duration", m_duration.frames(KdenliveSettings::defaultfps()));
                avfile.setAttribute("filesize", m_filesize);
		avfile.setAttribute("transparency", m_alphaTransparency);

		if (m_clipType == DocClipBase::SLIDESHOW) {
		    avfile.setAttribute("crossfade", m_hasCrossfade);
		    avfile.setAttribute("loop", m_loop);
		    avfile.setAttribute("ttl", m_ttl);
		    avfile.setAttribute("lumafile", m_luma);
		    avfile.setAttribute("lumasoftness", m_lumasoftness);
		    avfile.setAttribute("lumaduration", m_lumaduration);
		}

		avfile.setAttribute("channels", m_channels);
		avfile.setAttribute("frequency", m_frequency);
		if (m_clipType == DocClipBase::COLOR) avfile.setAttribute("color", m_color);
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
bool DocClipAVFile::matchesXML(const QDomElement & element) const
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

QString DocClipAVFile::formattedMetaData() 
{
	QString result;
	QMap<QString, QString>::Iterator it;
        for ( it = m_metadata.begin(); it != m_metadata.end(); ++it ) {
                    result+="<b>" + it.key() + "</b>: " + it.data() + "<br>";
        }
	return result;
}


/** Calculates properties for this file that will be useful for the rest of the program. */
void DocClipAVFile::calculateFileProperties(const QMap < QString,
    QString > &attributes, const QMap < QString,
    QString > &metadata)
{
    if (m_url.isLocalFile()) {
	QFileInfo fileInfo(m_url.path());

	/* Determines the size of the file */
	m_filesize = fileInfo.size();

	if (attributes.contains("duration")) {
	    m_duration = GenTime(attributes["duration"].toInt(), KdenliveSettings::defaultfps());
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
	    else if (attributes["type"] == "playlist")
		m_clipType = PLAYLIST;
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
	if (attributes.contains("videocodec")) {
	    m_videoCodec = attributes["videocodec"];
	}
	if (attributes.contains("audiocodec")) {
	    m_audioCodec = attributes["audiocodec"];
	}

	m_metadata = metadata;

	if (m_metadata.contains("description")) {
	    setDescription (m_metadata["description"]);
	}
	else if (m_metadata.contains("comment")) {
	    setDescription (m_metadata["comment"]);
	} 

    } else {
		/** If the file is not local, then no file properties are currently returned */
	m_duration = GenTime(0.0);
	m_durationKnown = false;
	m_framesPerSecond = 0;
	m_filesize = 0;
    }
}

QStringList DocClipAVFile::brokenPlayList()
{
    if (clipType() != PLAYLIST) return QStringList();
    QStringList missingClips;
    QDomDocument doc;
    doc.setContent(&QFile(m_url.path()), false);
    QDomElement documentElement = doc.documentElement();
    if (documentElement.tagName() != "westley") {
	kdWarning() << "KdenliveDoc::loadFromXML() document element has unknown tagName : " << documentElement.tagName() << endl;
    }

    QDomNode kdenlivedoc = documentElement.elementsByTagName("kdenlivedoc").item(0);
    QDomNode n;
    if (!kdenlivedoc.isNull()) n = kdenlivedoc.firstChild();
    else n = documentElement.firstChild();
    QDomElement e;
    bool missingFiles = false;

    while (!n.isNull()) {
	e = n.toElement();
	if (!e.isNull() && e.tagName() == "producer") {
	    QString prodUrl = e.attribute("resource", QString::null);
	    if (!prodUrl.isEmpty() && !KIO::NetAccess::exists(KURL(prodUrl), false)) 
		missingClips.append(prodUrl);
	}
        n = n.nextSibling();
    }
    return missingClips;
}
