/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstractprojectclip.h"
#include "producerwrapper.h"
#include "projectfolder.h"
#include "project.h"
#include "binmodel.h"
#include "monitor/monitorview.h"
#include "monitor/monitormanager.h"
#include <mlt++/Mlt.h>
#include <QDomElement>
#include <KDebug>

AbstractProjectClip::AbstractProjectClip(const KUrl& url, const QString &id, ProjectFolder* parent, AbstractClipPlugin const *plugin) :
    AbstractProjectItem(url, parent)
    , m_plugin(plugin)
    , m_url(url)
    , m_id(id),
    m_timelineBaseProducer(0)
{
}

AbstractProjectClip::AbstractProjectClip(ProducerWrapper* producer, ProjectFolder* parent, AbstractClipPlugin const *plugin) :
    AbstractProjectItem(KUrl(producer->get("resource")), parent),
    m_plugin(plugin),
    m_baseProducer(producer)
{
    char *resource = producer->get("resource");
    if (resource) {
        m_url = KUrl(resource);
        if (m_url.isValid()) {
            m_name = m_url.fileName();
        }
    }
}

AbstractProjectClip::AbstractProjectClip(const QDomElement& description, ProjectFolder* parent, AbstractClipPlugin const *plugin) :
    AbstractProjectItem(description, parent),
    m_plugin(plugin)
{
    Q_ASSERT(description.hasAttribute("id"));
    m_id = description.attribute("id");
    
    QString str;
    QTextStream stream(&str);
    description.save(stream, 4);
    m_baseProducer = new ProducerWrapper(*(bin()->project()->profile()), str, "xml-string");
    
    //m_url = KUrl(description.attribute("url"));
    m_url = KUrl(getXmlProperty(description, "resource"));
    if (description.hasAttribute("zone"))
	m_zone = QPoint(description.attribute("zone").section(':', 0, 0).toInt(), description.attribute("zone").section(':', 1, 1).toInt());
}


AbstractProjectClip::~AbstractProjectClip()
{
    // ?
    delete m_baseProducer;
    if (m_timelineBaseProducer) {
        delete m_timelineBaseProducer;
    }
}

QString AbstractProjectClip::getXmlProperty(const QDomElement &producer, const QString &propertyName)
{
    QString value;
    QDomNodeList props = producer.elementsByTagName("property");
    for (int i = 0; i < props.count(); ++i) {
        if (props.at(i).toElement().attribute("name") == propertyName) {
            value = props.at(i).firstChild().nodeValue();
            break;
        }
    }
    return value;
}

AbstractClipPlugin const *AbstractProjectClip::plugin() const
{
    return m_plugin;
}

QString AbstractProjectClip::clipId() const
{
    return m_id;
}

AbstractProjectClip* AbstractProjectClip::clip(const QString &id)
{
    if (id == m_id) {
        return this;
    }
    return NULL;
}

AbstractProjectClip* AbstractProjectClip::clipAt(int ix)
{
    if (ix == index()) {
        return this;
    }
    return NULL;
}

QString AbstractProjectClip::id() const
{
    return m_id;
}

bool AbstractProjectClip::hasUrl() const
{
    return m_url.isValid();
}

KUrl AbstractProjectClip::url() const
{
    return m_url;
}

bool AbstractProjectClip::hasLimitedDuration() const
{
    return m_hasLimitedDuration;
}

int AbstractProjectClip::duration() const
{
    return m_baseProducer->get_playtime();
}

QString AbstractProjectClip::serializeClip()
{
    Mlt::Consumer *consumer = bin()->project()->xmlConsumer();
    consumer->connect(*m_baseProducer);
    consumer->run();
    return QString::fromUtf8(consumer->get("kdenlive_clip"));
}

void AbstractProjectClip::reloadProducer()
{
    if (!m_baseProducer) {
	initProducer();
	return;
    }
    QString playlist = serializeClip();
    if (playlist.isEmpty()) return;
    ProducerWrapper *copy = new ProducerWrapper(*(m_baseProducer->profile()), playlist, "xml-string");
    delete m_baseProducer;
    m_baseProducer = copy;
}

void AbstractProjectClip::setCurrent(bool current, bool notify)
{
    AbstractProjectItem::setCurrent(current, notify);
    if (current) {
	initProducer();
	kDebug()<< " * * *SET CURRENT CLP: "<<m_name;
        bin()->monitor()->open(m_baseProducer, ClipMonitor, m_zone);
    }
}

QDomElement AbstractProjectClip::toXml(QDomDocument& document)
{
    QDomDocument mltData;
    QString playlist = serializeClip();
    mltData.setContent(playlist);
    QDomElement clip = document.importNode(mltData.documentElement().firstChildElement("producer"), true).toElement();
    clip.setAttribute("duration", m_baseProducer->get_length());
    clip.setAttribute("producer_type",m_baseProducer->get("mlt_service"));
    clip.setAttribute("name", name());
    if (!m_zone.isNull()) {
	clip.setAttribute("zone", QString::number(m_zone.x()) + ":" + QString::number(m_zone.y()));
    }
    if (!description().isEmpty()) {
        clip.setAttribute("description", description());
    }
    //clip.setAttribute("url", m_url.path());
    
    // We change the tag so that MLT is not confused when trying to play the .kdenlive project file
    clip.setTagName("clip");
    return clip;
}

ProducerWrapper* AbstractProjectClip::timelineBaseProducer()
{
    if (!m_timelineBaseProducer) {
        if (!m_baseProducer) {
            initProducer();
            m_timelineBaseProducer = new ProducerWrapper(*m_baseProducer->profile(), m_baseProducer->property("resource"));
            // copy/set properties? if yes then pass_list and/or pass_values should be used
        }
    }
    return m_timelineBaseProducer;
}

void AbstractProjectClip::setTimelineBaseProducer(ProducerWrapper* producer)
{
    m_timelineBaseProducer = producer;
}

ProducerWrapper* AbstractProjectClip::baseProducer()
{
    if (!m_baseProducer)
	initProducer();
    return m_baseProducer;
}

QPixmap AbstractProjectClip::thumbnail(bool force)
{
    if ((force || m_thumbnail.isNull()) && m_baseProducer) {
	int width = 80 * bin()->project()->displayRatio();
	if (width % 2 == 1) width++;
	bin()->project()->monitorManager()->requestThumbnails(m_id, QList <int>() << 0);
    }
    return m_thumbnail;
}

void AbstractProjectClip::setThumbnail(QImage img)
{
    m_thumbnail = roundedPixmap(QPixmap::fromImage(img)); //QPixmap::fromImage(img);
    bin()->emitItemUpdated(this);
}

void AbstractProjectClip::setZone(const QPoint &zone)
{
    m_zone = zone;
}

QPoint AbstractProjectClip::zone() const
{
    return m_zone;
}

void AbstractProjectClip::addMarker(int position)
{
    m_markers << position;
    bin()->markersUpdated(m_id, m_markers);
}

void AbstractProjectClip::removeMarker(int position)
{
    m_markers.removeAll(position);
    bin()->markersUpdated(m_id, m_markers);
}

#include "abstractprojectclip.moc"
