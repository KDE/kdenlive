/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "videoprojectclip.h"
#include "videoclipplugin.h"
#include "videotimelineclip.h"
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


VideoProjectClip::VideoProjectClip(const KUrl& url, const QString &id, ProjectFolder* parent, VideoClipPlugin const *plugin) :
    AbstractProjectClip(url, id, parent, plugin)
{
    m_baseProducer = NULL;
    initProducer();
    init();
}

VideoProjectClip::VideoProjectClip(ProducerWrapper* producer, ProjectFolder* parent, VideoClipPlugin const *plugin) :
    AbstractProjectClip(producer, parent, plugin)
{
    init();
}

VideoProjectClip::VideoProjectClip(const QDomElement& description, ProjectFolder* parent, VideoClipPlugin const *plugin) :
    AbstractProjectClip(description, parent, plugin)
{
    Q_ASSERT(description.attribute("producer_type") == "avformat");

    m_baseProducer = new ProducerWrapper(*(bin()->project()->profile()), m_url.path());

    Q_ASSERT(m_baseProducer->property("mlt_service") == description.attribute("producer_type"));

    kDebug() << "video project clip created" << id();

    init();
}

VideoProjectClip::~VideoProjectClip()
{
}

AbstractTimelineClip* VideoProjectClip::addInstance(ProducerWrapper* producer, TimelineTrack* parent)
{
    VideoTimelineClip *instance = new VideoTimelineClip(producer, this, parent);
    m_instances.append(instance);
    return static_cast<AbstractTimelineClip *>(instance);
}


QPixmap VideoProjectClip::thumbnail()
{
    if (m_thumbnail.isNull() && m_baseProducer) {
        m_thumbnail = roundedPixmap(m_baseProducer->pixmap(0, 80 * bin()->project()->displayRatio(), 80));
    }
    
    return m_thumbnail;
}

void VideoProjectClip::hash()
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

void VideoProjectClip::initProducer()
{
    if (!m_baseProducer)
	m_baseProducer = new ProducerWrapper(*bin()->project()->profile(), url().path());
}

void VideoProjectClip::init()
{
    //Q_ASSERT(m_baseProducer && m_baseProducer->is_valid());
    //Q_ASSERT(m_baseProducer->property("mlt_service") == "avformat");
    hash();
    m_hasLimitedDuration = true;
    m_duration = Timecode(m_baseProducer->get_length()).formatted();
    thumbnail();

    //kDebug() << "new project clip created " << m_baseProducer->get("resource") << m_baseProducer->get_length();
}




#include "videoprojectclip.moc"
