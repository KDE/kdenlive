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
#include "core/monitor/monitormanager.h"
#include "core/project/binmodel.h"
#include "core/timecodeformatter.h"
#include "core/timecode.h"
#include "src/core/kdenlivesettings.h"
#include <mlt++/Mlt.h>
#include <KUrl>
#include <KFileMetaInfo>
#include <QDomElement>
#include <QFile>
#include <QTimer>
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

ImageProjectClip::ImageProjectClip(const QString& service, Mlt::Properties props, const QString &id, ProjectFolder* parent, ImageClipPlugin const *plugin) :
    AbstractProjectClip(KUrl(props.get("resource")), id, parent, plugin)
{
    Q_ASSERT(service == "pixbuf" || service == "qimage");
    m_baseProducer = NULL;
    initProducer(service, props);
    init();
}

ImageProjectClip::ImageProjectClip(const QDomElement& description, ProjectFolder* parent, ImageClipPlugin const *plugin) :
    AbstractProjectClip(description, parent, plugin)
{
    Q_ASSERT(description.attribute("producer_type") == "pixbuf" || description.attribute("producer_type") == "qimage");

    //m_baseProducer = new ProducerWrapper(*(bin()->project()->profile()), m_url.path());

    Q_ASSERT(m_baseProducer->property("mlt_service") == description.attribute("producer_type"));

    kDebug() << "image project clip created" << clipId();

    init(description.attribute("duration", "0").toInt());
}

ImageProjectClip::~ImageProjectClip()
{
}

AbstractTimelineClip* ImageProjectClip::createInstance(TimelineTrack* parent, ProducerWrapper* producer)
{
    ImageTimelineClip *instance = new ImageTimelineClip(this, parent, producer);
    m_instances.append(instance);

    if (producer) {
        setTimelineBaseProducer(new ProducerWrapper(&producer->parent()));
    }

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


QPixmap ImageProjectClip::thumbnail()
{
    if (m_thumbnail.isNull() && m_baseProducer) {
        int width = 80 * bin()->project()->displayRatio();
        if (width % 2 == 1)
            width++;
        bin()->project()->monitorManager()->requestThumbnails(m_id, QList <int>() << 0);
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

    kDebug() << "new project clip created " << m_baseProducer->get("resource") << m_baseProducer->get_length();
    QTimer::singleShot(0, this, SLOT(thumbnail()));
}

void ImageProjectClip::initProducer()
{
    if (!m_baseProducer) {
        m_baseProducer = new ProducerWrapper(*bin()->project()->profile(), url().path());
        m_baseProducer->set("id", clipId().toUtf8().constData());
    }
}

void ImageProjectClip::initProducer(const QString &service, Mlt::Properties props)
{
    if (!m_baseProducer) {
        m_baseProducer = new ProducerWrapper(*bin()->project()->profile(), props.get("resource"), service);
        //TODO: pass all properties to producer
        m_baseProducer->set("id", clipId().toUtf8().constData());
    }
}

#include "imageprojectclip.moc"
