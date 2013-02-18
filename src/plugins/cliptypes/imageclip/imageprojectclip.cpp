/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "imageprojectclip.h"
#include "imageclipplugin.h"
#include "imagetimelineclip.h"
#include "core/project/projectfolder.h"
#include "core/project/producerwrapper.h"
#include "core/project/project.h"
#include "core/project/binmodel.h"
#include "core/timecodeformatter.h"
#include "core/timecode.h"
#include "src/core/kdenlivesettings.h"
#include <mlt++/Mlt.h>
#include <KUrl>
#include <KFileMetaInfo>
#include <QDomElement>
#include <QFile>
#include <QCryptographicHash>


ImageProjectClip::ImageProjectClip(const KUrl& url, const QString &id, ProjectFolder* parent, ImageClipPlugin const *plugin) :
    AbstractProjectClip(url, id, parent, plugin)
{
    m_baseProducer = NULL;
    initProducer();
    init();
}

ImageProjectClip::ImageProjectClip(ProducerWrapper* producer, ProjectFolder* parent, ImageClipPlugin const *plugin) :
    AbstractProjectClip(producer, parent, plugin)
{
    init();
}

ImageProjectClip::ImageProjectClip(const QDomElement& description, ProjectFolder* parent, ImageClipPlugin const *plugin) :
    AbstractProjectClip(description, parent, plugin)
{
    Q_ASSERT(description.attribute("producer_type") == "pixbuf" || description.attribute("producer_type") == "qimage");

    m_baseProducer = new ProducerWrapper(*(bin()->project()->profile()), m_url.path());

    Q_ASSERT(m_baseProducer->property("mlt_service") == description.attribute("producer_type"));

    kDebug() << "image project clip created" << id();

    init(description.attribute("duration", "0").toInt());
}

ImageProjectClip::~ImageProjectClip()
{
}

AbstractTimelineClip* ImageProjectClip::addInstance(ProducerWrapper* producer, TimelineTrack* parent)
{
    ImageTimelineClip *instance = new ImageTimelineClip(producer, this, parent);
    m_instances.append(instance);
    return static_cast<AbstractTimelineClip *>(instance);
}

void ImageProjectClip::hash()
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

QDomElement ImageProjectClip::toXml(QDomDocument& document) const
{
    QDomElement clip = AbstractProjectClip::toXml(document);
    clip.setAttribute("duration", m_baseProducer->get_length());
    return clip;
}

QPixmap ImageProjectClip::thumbnail()
{
    if (m_thumbnail.isNull() && m_baseProducer) {
        m_thumbnail = m_baseProducer->pixmap().scaledToHeight(100, Qt::SmoothTransformation);
    }
    return m_thumbnail;
}

void ImageProjectClip::init(int duration)
{
    Q_ASSERT(m_baseProducer && m_baseProducer->is_valid());
    Q_ASSERT(m_baseProducer->property("mlt_service") == "qimage" || m_baseProducer->property("mlt_service") == "pixbuf");

    m_hasLimitedDuration = false;
    hash();

    if (duration == 0) {
        duration = bin()->project()->timecodeFormatter()->fromString(KdenliveSettings::image_duration(), TimecodeFormatter::HH_MM_SS_FF).frames();
    }
    m_baseProducer->set("length", duration);
    m_baseProducer->set_in_and_out(0, -1);
    thumbnail();

    kDebug() << "new project clip created " << m_baseProducer->get("resource") << m_baseProducer->get_length();
}

void ImageProjectClip::initProducer()
{
    if (!m_baseProducer)
	m_baseProducer = new ProducerWrapper(*bin()->project()->profile(), url().path());
}


#include "imageprojectclip.moc"
