/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "videoclipplugin.h"
#include "videoprojectclip.h"
#include "videotimelineclip.h"
#include "videotimelineclipitem.h"
#include "core/project/clippluginmanager.h"
#include "core/project/projectfolder.h"
#include "core/project/abstracttimelineclip.h"
#include "core/producersystem/producerdescription.h"
#include <KPluginFactory>
#include <KLocale>
#include <QDomElement>

K_PLUGIN_FACTORY( VideoClipFactory, registerPlugin<VideoClipPlugin>(); )
K_EXPORT_PLUGIN( VideoClipFactory( "kdenlivevideoclip" ) )

VideoClipPlugin::VideoClipPlugin(QObject* parent, const QVariantList& args) :
    AbstractClipPlugin(static_cast<ClipPluginManager*>(parent))
{
    Q_UNUSED(args)

    ClipPluginManager *clipPluginManager = static_cast<ClipPluginManager*>(parent);

    clipPluginManager->addSupportedMimetypes(QStringList() << "video/mp2t" << "video/x-flv" << "application/vnd.rn-realmedia" << "video/x-dv" << "video/dv" << "video/x-msvideo"
                                                           << "video/x-matroska" << "video/mpeg" << "video/ogg" << "video/x-ms-wmv" << "video/mp4" << "video/quicktime" << "video/webm"
                                                           << "audio/x-flac" << "audio/x-matroska" << "audio/mp4" << "audio/mpeg" << "audio/x-mp3" << "audio/ogg" << "audio/x-wav"
                                                           << "audio/x-aiff" << "audio/aiff" << "application/ogg" << "application/mxf" << "application/x-shockwave-flash");
}

VideoClipPlugin::~VideoClipPlugin()
{
    
}

AbstractProjectClip* VideoClipPlugin::createClip(const KUrl &url, const QString &id, ProjectFolder *parent) const
{
    VideoProjectClip *projectClip = new VideoProjectClip(url, id, parent, this);
    return projectClip;
}

AbstractProjectClip* VideoClipPlugin::createClip(const QString &service, Mlt::Properties props, const QString &id, ProjectFolder *parent) const
{
    VideoProjectClip *projectClip = new VideoProjectClip(service, props, id, parent, this);
    return projectClip;
}

AbstractProjectClip* VideoClipPlugin::loadClip(const QDomElement& description, ProjectFolder *parent) const
{
    VideoProjectClip *projectClip = new VideoProjectClip(description, parent, this);
    return projectClip;
}

QString VideoClipPlugin::nameForService(const QString &) const
{
    return i18n("Video Clip");
}

void VideoClipPlugin::fillDescription(Mlt::Properties properties, ProducerDescription *description)
{
    int streams = properties.get_int("meta.media.nb_streams");
    //description->setParameterValue("audio_index", "maximum", streams);
    //description->setParameterValue("video_index", "maximum", streams);
}

TimelineClipItem* VideoClipPlugin::timelineClipView(AbstractTimelineClip* clip, QGraphicsItem* parent) const
{
    return new VideoTimelineClipItem(static_cast<VideoTimelineClip*>(clip), parent);
}

#include "videoclipplugin.moc"
