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
#include "core/project/producerwrapper.h"
#include "src/core/kdenlivesettings.h"
#include <mlt++/Mlt.h>
#include <KUrl>
#include <KFileMetaInfo>
#include <QPixmap>
#include <QDomElement>


ImageProjectClip::ImageProjectClip(const KUrl& url, QObject* parent) :
    AbstractProjectClip(url, parent)
{
    m_hasLimitedDuration = false;

    Mlt::Profile profile(KdenliveSettings::current_profile().toUtf8().constData());
    m_baseProducer = new ProducerWrapper(profile, url);

    init();
}

ImageProjectClip::ImageProjectClip(ProducerWrapper* producer, QObject* parent) :
    AbstractProjectClip(producer, parent)
{
    init();
}

ImageProjectClip::ImageProjectClip(const QDomElement& description, QObject* parent) :
    AbstractProjectClip(description, parent)
{
    Q_ASSERT(description.attribute("producer_type") == "pixbuf" || description.attribute("producer_type") == "qimage");

    Mlt::Profile profile(KdenliveSettings::current_profile().toUtf8().constData());
    m_baseProducer = new ProducerWrapper(profile, m_url);

    kDebug() << "image project clip created" << id();

    // set duration...
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

void ImageProjectClip::init()
{
    Q_ASSERT(m_baseProducer && m_baseProducer->is_valid());
    Q_ASSERT(m_baseProducer->property("mlt_service") == "qimage" || m_baseProducer->property("mlt_service") == "pixbuf");

    m_baseProducer->setProperty("length", KdenliveSettings::image_duration());
    m_baseProducer->set_in_and_out(0, KdenliveSettings::image_duration().toInt());

    m_thumbnail = 0;

    kDebug() << "new project clip created " << m_baseProducer->get("resource") << m_baseProducer->get_length() << m_baseProducer->position();
    QPixmap *pix = thumbnail();
    pix->save("test.png");
    //...
}




#include "imageprojectclip.moc"
