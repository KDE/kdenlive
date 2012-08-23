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


VideoProjectClip::VideoProjectClip(const KUrl& url, ProjectFolder* parent, VideoClipPlugin const *plugin) :
    AbstractProjectClip(url, parent, plugin)
{
    m_baseProducer = new ProducerWrapper(*bin()->project()->profile(), url.path());

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
    if (m_thumbnail.isNull()) {
        m_thumbnail = m_baseProducer->pixmap().scaledToHeight(100, Qt::SmoothTransformation);
    }
    return m_thumbnail;
}

void VideoProjectClip::init()
{
    Q_ASSERT(m_baseProducer && m_baseProducer->is_valid());
    Q_ASSERT(m_baseProducer->property("mlt_service") == "avformat");

    m_hasLimitedDuration = true;
    thumbnail();

    kDebug() << "new project clip created " << m_baseProducer->get("resource") << m_baseProducer->get_length();
}




#include "videoprojectclip.moc"
