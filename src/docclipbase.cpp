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

#include <KDebug>

#include "kdenlivesettings.h"
#include "docclipbase.h"

DocClipBase::DocClipBase(QDomElement xml, uint id):
m_xml(xml), m_id(id), m_description(""), m_refcount(0), m_projectThumbFrame(0), audioThumbCreated(false), m_duration(GenTime()), m_thumbProd(NULL)
{
  int type = xml.attribute("type").toInt();
  m_clipType = (CLIPTYPE) type;
  m_name = xml.attribute("name");
  m_xml.setAttribute("id", QString::number(id));
  KUrl url = KUrl(xml.attribute("resource"));
  int out = xml.attribute("out").toInt();
  if (out != 0) setDuration(GenTime(out, 25));
  if (m_name.isEmpty()) m_name = url.fileName();
  if (!url.isEmpty())
    m_thumbProd = new KThumb(url, KdenliveSettings::track_height() * KdenliveSettings::project_display_ratio(), KdenliveSettings::track_height());
}

DocClipBase::DocClipBase(const DocClipBase& clip)
{
    m_xml = clip.toXML();
    m_id = clip.getId();
    m_clipType = clip.clipType();
    m_name = clip.name();
    m_duration = clip.duration();
}

DocClipBase & DocClipBase::operator=(const DocClipBase & clip)
{
    DocClipBase::operator=(clip);
    m_xml = clip.toXML();
    m_id = clip.getId();
    m_clipType = clip.clipType();
    m_name = clip.name();
    m_duration = clip.duration();
    return *this;
}

DocClipBase::~DocClipBase()
{
  //if (m_thumbProd) delete m_thumbProd;
}

KThumb *DocClipBase::thumbProducer()
{
  return m_thumbProd;
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

const CLIPTYPE & DocClipBase::clipType() const
{
  return m_clipType;
}

void DocClipBase::setClipType(CLIPTYPE type)
{
  m_clipType = type;
}

const KUrl & DocClipBase::fileURL() const
{
  QString res = m_xml.attribute("resource");
  if (m_clipType != COLOR && !res.isEmpty()) return KUrl(res);
  return KUrl();
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

void DocClipBase::setDuration(GenTime dur)
{
    m_duration = dur;
}

const GenTime &DocClipBase::duration() const
{
    return m_duration;
}

bool DocClipBase::hasFileSize() const
{
  return true;
}


// virtual
QDomElement DocClipBase::toXML() const
{
/*
    QDomDocument doc;

    QDomElement clip = doc.createElement("kdenliveclip");
    QDomText text = doc.createTextNode(description());
    clip.appendChild(text);
    doc.appendChild(clip);
*/
    return m_xml;
}

DocClipBase *DocClipBase::
createClip(KdenliveDoc *doc, const QDomElement & element)
{
    DocClipBase *clip = 0;
    QString description;
    QDomNode node = element;
    node.normalize();
    if (element.tagName() != "kdenliveclip") {
	kWarning() <<
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
		// clip = DocClipAVFile::createClip(e);
	    } else if (e.tagName() == "DocTrackBaseList") {
		// clip = DocClipProject::createClip(doc, e);
	    }
	} else {
	    QDomText text = n.toText();
	    if (!text.isNull()) {
		description = text.nodeValue();
	    }
	}

	n = n.nextSibling();
    }
    if (clip == 0) {
	kWarning() << "DocClipBase::createClip() unable to create clip" <<
	    endl;
    } else {
	// setup DocClipBase specifics of the clip.
	clip->setDescription(description);
    	clip->setAudioThumbCreated(false);
    }
    return clip;
}

void DocClipBase::setAudioThumbCreated(bool isDone)
{
    audioThumbCreated = isDone;
}


QDomDocument DocClipBase::generateSceneList(bool, bool) const
{
}

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

QList < GenTime > DocClipBase::snapMarkers() const
{
    QList < GenTime > markers;

    for (uint count = 0; count < m_snapMarkers.count(); ++count) {
	markers.append(m_snapMarkers[count].time());
    }

    return markers;
}

QList < CommentedTime > DocClipBase::commentedSnapMarkers() const
{
    return m_snapMarkers;
}

void DocClipBase::setSnapMarkers(QList < CommentedTime > markers)
{
    m_snapMarkers = markers;
}

void DocClipBase::addSnapMarker(const GenTime & time, QString comment)
{
    QList < CommentedTime >::Iterator it = m_snapMarkers.begin();
    for ( it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it ) {
	if ((*it).time() >= time)
	    break;
    }

    if ((it != m_snapMarkers.end()) && ((*it).time() == time)) {
	kError() <<
	    "trying to add Snap Marker that already exists, this will cause inconsistancies with undo/redo"
	    << endl;
    } else {
	CommentedTime t(time, comment);
	m_snapMarkers.insert(it, t);
    }

}

void DocClipBase::editSnapMarker(const GenTime & time, QString comment)
{
    QList < CommentedTime >::Iterator it;
    for ( it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it ) {
	if ((*it).time() == time)
	    break;
    }
    if (it != m_snapMarkers.end()) {
	(*it).setComment(comment);
    } else {
	kError() <<
	    "trying to edit Snap Marker that does not already exists"  << endl;
    }
}

QString DocClipBase::deleteSnapMarker(const GenTime & time)
{
    QString result = i18n("Marker");
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if ((*itt).time() == time)
	    break;
	++itt;
    }

    if ((itt != m_snapMarkers.end()) && ((*itt).time() == time)) {
	result = (*itt).comment();
	m_snapMarkers.erase(itt);
    }
    return result;
}


GenTime DocClipBase::hasSnapMarkers(const GenTime & time)
{
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

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
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
	if ((*itt).time() == t)
	    return (*itt).comment();
	++itt;
    }
    return QString::null;
}

//static
QString DocClipBase::getTypeName(CLIPTYPE type)
{
    QString result;
    switch (type) {
	case AV:
	    result = i18n("Video Clip");
	    break;
	case COLOR:
	    result = i18n("Color Clip");
	    break;
	case PLAYLIST:
	    result = i18n("Playlist Clip");
	    break;
	case IMAGE:
	    result = i18n("Image Clip");
	    break;
	case SLIDESHOW:
	    result = i18n("Slideshow Clip");
	    break;
	case VIRTUAL:
	    result = i18n("Virtual Clip");
	    break;
	case AUDIO:
	    result = i18n("Audio Clip");
	    break;
	case VIDEO:
	    result = i18n("Mute Video Clip");
	    break;
	case TEXT:
	    result = i18n("Text Clip");
	    break;
	default:
	    result = i18n("None");
	    break;
    }
    return result;
}


