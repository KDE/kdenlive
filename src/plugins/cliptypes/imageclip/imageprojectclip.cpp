/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "imageprojectclip.h"
#include "imagetimelineclip.h"
#include "core/project/projectfolder.h"
#include "core/project/producerwrapper.h"
#include "core/project/project.h"
#include "core/project/binmodel.h"
#include "src/core/kdenlivesettings.h"
#include <mlt++/Mlt.h>
#include <KUrl>
#include <KFileMetaInfo>
#include <QPixmap>
#include <QDomElement>


ImageProjectClip::ImageProjectClip(const KUrl& url, ProjectFolder* parent) :
    AbstractProjectClip(url, parent)
{
    m_baseProducer = new ProducerWrapper(*bin()->project()->profile(), url.path());

    init();
}

ImageProjectClip::ImageProjectClip(ProducerWrapper* producer, ProjectFolder* parent) :
    AbstractProjectClip(producer, parent)
{
    init();
}

ImageProjectClip::ImageProjectClip(const QDomElement& description, ProjectFolder* parent) :
    AbstractProjectClip(description, parent)
{
    Q_ASSERT(description.attribute("producer_type") == "pixbuf" || description.attribute("producer_type") == "qimage");

    m_baseProducer = new ProducerWrapper(*(bin()->project()->profile()), m_url.path());

    Q_ASSERT(m_baseProducer->property("mlt_service") == description.attribute("producer_type"));

    kDebug() << "image project clip created" << id();

    init(description.attribute("duration", "0").toInt());
}

ImageProjectClip::~ImageProjectClip()
{
    if (m_thumbnail) {
        delete m_thumbnail;
    }
}

AbstractTimelineClip* ImageProjectClip::addInstance(ProducerWrapper* producer, TimelineTrack* parent)
{
    ImageTimelineClip *instance = new ImageTimelineClip(producer, this, parent);
    m_instances.append(instance);
    return static_cast<AbstractTimelineClip *>(instance);
}


QPixmap *ImageProjectClip::thumbnail()
{
    if (!m_thumbnail) {
        m_thumbnail = m_baseProducer->pixmap();
    }
    return m_thumbnail;
}

void ImageProjectClip::init(int duration)
{
    Q_ASSERT(m_baseProducer && m_baseProducer->is_valid());
    Q_ASSERT(m_baseProducer->property("mlt_service") == "qimage" || m_baseProducer->property("mlt_service") == "pixbuf");

    m_hasLimitedDuration = false;

    if (duration == 0) {
        // requires timecode parsing first
        duration = 125; //KdenliveSettings::image_duration().toInt();
    }
    m_baseProducer->set("length", duration);
    m_baseProducer->set_in_and_out(0, -1);

    m_thumbnail = 0;

    kDebug() << "new project clip created " << m_baseProducer->get("resource") << m_baseProducer->get_length();
//     QPixmap *pix = thumbnail();
//     pix->save("test.png");
    //...
}




#include "imageprojectclip.moc"
