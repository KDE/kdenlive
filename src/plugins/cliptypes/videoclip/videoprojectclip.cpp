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
#include "core/monitor/monitorview.h"
#include "core/monitor/monitormanager.h"
#include "core/monitor/mltcontroller.h"
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
#include <QTimer>
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

VideoProjectClip::VideoProjectClip(const QString& service, Mlt::Properties props, const QString &id, ProjectFolder* parent, VideoClipPlugin const *plugin) :
    AbstractProjectClip(KUrl(props.get("resource")), id, parent, plugin)
{
    Q_ASSERT(service == "avformat");
    m_baseProducer = NULL;
    initProducer(service, props);
    init();
}

VideoProjectClip::VideoProjectClip(const QDomElement& description, ProjectFolder* parent, VideoClipPlugin const *plugin) :
    AbstractProjectClip(description, parent, plugin)
{
    Q_ASSERT(description.attribute("producer_type") == "avformat");

    //m_baseProducer = new ProducerWrapper(*(bin()->project()->profile()), m_url.path());

    Q_ASSERT(m_baseProducer->property("mlt_service") == description.attribute("producer_type"));

    kDebug() << "video project clip created" << clipId();

    init();
}

VideoProjectClip::~VideoProjectClip()
{
}

AbstractTimelineClip* VideoProjectClip::createInstance(TimelineTrack* parent, ProducerWrapper* producer)
{
    VideoTimelineClip *instance = new VideoTimelineClip(this, parent, producer);
    m_instances.append(instance);

    if (producer) {
        setTimelineBaseProducer(new ProducerWrapper(&producer->parent()));
    }

    return static_cast<AbstractTimelineClip *>(instance);
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
    if (!m_baseProducer) {
        m_baseProducer = new ProducerWrapper(*bin()->project()->profile(), url().path());
        m_baseProducer->set("id", clipId().toUtf8().constData());
    }
}

void VideoProjectClip::init()
{
    //Q_ASSERT(m_baseProducer && m_baseProducer->is_valid());
    //Q_ASSERT(m_baseProducer->property("mlt_service") == "avformat");
    hash();
    m_hasLimitedDuration = true;
    Timecode t(m_baseProducer->get_length(), bin()->project()->timecodeFormatter());
    m_duration = t.formatted();
    QTimer::singleShot(0, this, SLOT(thumbnail()));

    kDebug() << "new project clip created " << m_baseProducer->get("resource") << m_baseProducer->get_length();
}

void VideoProjectClip::initProducer(const QString &service, Mlt::Properties props)
{
    if (!m_baseProducer) {
        m_baseProducer = new ProducerWrapper(*bin()->project()->profile(), props.get("resource"), service);
        //TODO: pass all properties to producer
        m_baseProducer->set("id", clipId().toUtf8().constData());
    }
}


#include "videoprojectclip.moc"
