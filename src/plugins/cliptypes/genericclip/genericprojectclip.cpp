/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "genericprojectclip.h"
#include "genericclipplugin.h"
#include "generictimelineclip.h"
#include "core/monitor/monitormanager.h"
#include "core/project/projectfolder.h"
#include "core/project/producerwrapper.h"
#include "core/project/project.h"
#include "core/project/binmodel.h"
#include "core/timecodeformatter.h"
#include "core/timecode.h"
#include "core/core.h"
#include "src/core/kdenlivesettings.h"
#include "core/producersystem/producerrepository.h"
#include <mlt++/Mlt.h>
#include <KUrl>
#include <KFileMetaInfo>
#include <QDomElement>
#include <QFile>
#include <QTimer>
#include <QCryptographicHash>


GenericProjectClip::GenericProjectClip(const KUrl& url, const QString &id, ProjectFolder* parent, GenericClipPlugin const *plugin) :
    AbstractProjectClip(url, id, parent, plugin)
{
    m_baseProducer = NULL;
    initProducer();
    init();
}

GenericProjectClip::GenericProjectClip(ProducerWrapper* producer, ProjectFolder* parent, GenericClipPlugin const *plugin) :
    AbstractProjectClip(producer, parent, plugin)
{
    init();
}

GenericProjectClip::GenericProjectClip(const QString& service, Mlt::Properties props, const QString &id, ProjectFolder* parent, GenericClipPlugin const *plugin) :
    AbstractProjectClip(KUrl(props.get("resource")), id, parent, plugin)
{
    m_baseProducer = NULL;
    initProducer(service, props);
    init();
}

GenericProjectClip::GenericProjectClip(const QDomElement& description, ProjectFolder* parent, GenericClipPlugin const *plugin) :
    AbstractProjectClip(description, parent, plugin)
{
    //Q_ASSERT(description.attribute("producer_type") == "color" || description.attribute("producer_type") == "count");

    m_baseProducer = new ProducerWrapper(*(bin()->project()->profile()), description.attribute("producer_type") + QString(":%1").arg(url().path()));

    Q_ASSERT(m_baseProducer->property("mlt_service") == description.attribute("producer_type"));

    kDebug() << "generic project clip created" << id()<< " = "<<description.attribute("producer_type") + QString(":%1").arg(url().path());
    
    init(description.attribute("duration", "0").toInt(), description.attribute("in", "0").toInt(), description.attribute("out", "0").toInt());
    QStringList producerProperties = pCore->producerRepository()->producerProperties(description.attribute("producer_type"));
    QDomNodeList props = description.elementsByTagName("property");
    for (int i = 0; i < props.count(); i++) {
        QDomElement e = props.item(i).toElement();
        if (!e.isNull()) {
	    m_baseProducer->setProperty(e.attribute("name"), e.firstChild().nodeValue());
	}
    }
}

GenericProjectClip::~GenericProjectClip()
{
}

AbstractTimelineClip* GenericProjectClip::createInstance(TimelineTrack* parent, ProducerWrapper* producer)
{
    GenericTimelineClip *instance = new GenericTimelineClip(this, parent, producer);
    m_instances.append(instance);

    if (producer) {
        setTimelineBaseProducer(new ProducerWrapper(&producer->parent()));
    }

    return static_cast<AbstractTimelineClip *>(instance);
}

void GenericProjectClip::hash()
{
    if (m_hash.isEmpty() && hasUrl()) {
	QFile file(m_url.path());
	if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
	    QByteArray fileData;
	    //kDebug() << "SETTING HASH of" << value;
	    m_fileSize = file.size();
	    /*
               * 1 MB = 1 second per 450 files (or faster)
               * 10 MB = 9 seconds per 450 files (or faster)
               */
	    if (file.size() > 1000000*2) {
		fileData = file.read(1000000);
		if (file.seek(file.size() - 1000000))
		    fileData.append(file.readAll());
	    } else
		fileData = file.readAll();
	    file.close();
	    m_hash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
	}
    }
}

QDomElement GenericProjectClip::toXml(QDomDocument& document) const
{
    QDomElement clip = AbstractProjectClip::toXml(document);
    clip.setAttribute("duration", m_baseProducer->get_length());
    QStringList producerProperties = pCore->producerRepository()->producerProperties(clip.attribute("producer_type"));
    foreach(const QString &key, producerProperties) {
	QString value = m_baseProducer->property(key);
	if (!value.isEmpty()) {
	    QDomElement property = document.createElement("property");
	    property.setAttribute("name", key);
	    QDomText val = document.createTextNode(value);
	    property.appendChild(val);
	    clip.appendChild(property);
	}
    }
    return clip;
}

QPixmap GenericProjectClip::thumbnail()
{
    /*if (m_thumbnail.isNull() && m_baseProducer) {
        m_thumbnail = m_baseProducer->pixmap().scaledToHeight(100, Qt::SmoothTransformation);
    }*/
    if (m_thumbnail.isNull() && m_baseProducer) {
	int width = 80 * bin()->project()->displayRatio();
	if (width % 2 == 1) width++;
	bin()->project()->monitorManager()->requestThumbnails(m_id, QList <int>() << 0);
    }
    return m_thumbnail;
}

void GenericProjectClip::init(int duration, int in , int out)
{
    Q_ASSERT(m_baseProducer && m_baseProducer->is_valid());
    //Q_ASSERT(m_baseProducer->property("mlt_service") == "color" || m_baseProducer->property("mlt_service") == "count");

    m_hasLimitedDuration = false;
    hash();
    if (duration == 0) {
	duration = m_baseProducer->get_length();
    }
    if (duration == 0) {
	//TODO: replace color_duration with generic_duration
	duration = bin()->project()->timecodeFormatter()->fromString(KdenliveSettings::color_duration(), TimecodeFormatter::HH_MM_SS_FF).frames();
    }
    kDebug()<<" * * * *\nGENERIC CLIP: "<<duration<<", "<<in<<", "<<out;
    if (out == 0) out = duration - 1;
    m_baseProducer->set("length", duration);
    m_baseProducer->set_in_and_out(in, out);

    kDebug() << "new project clip created " << m_baseProducer->get("resource") << m_baseProducer->get_length();
    QTimer::singleShot(0, this, SLOT(thumbnail()));
}

void GenericProjectClip::initProducer()
{
    if (!m_baseProducer) {
	m_baseProducer = new ProducerWrapper(*bin()->project()->profile(), url().path());
	m_baseProducer->set("id", id().toUtf8().constData());
    }
}

void GenericProjectClip::initProducer(const QString &service, Mlt::Properties props)
{
    if (!m_baseProducer) {
	m_baseProducer = new ProducerWrapper(*bin()->project()->profile(), QString(service + ":" + props.get("resource")));
	//TODO: pass all properties to producer
	m_baseProducer->set("id", id().toUtf8().constData());
    }
}


#include "genericprojectclip.moc"
