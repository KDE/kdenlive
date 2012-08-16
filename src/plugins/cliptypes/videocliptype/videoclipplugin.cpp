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
#include <KPluginFactory>
#include <QDomElement>

K_PLUGIN_FACTORY( VideoClipFactory, registerPlugin<VideoClipPlugin>(); )
K_EXPORT_PLUGIN( VideoClipFactory( "kdenlivevideoclip" ) )

VideoClipPlugin::VideoClipPlugin(QObject* parent, const QVariantList& args) :
    AbstractClipPlugin(static_cast<ClipPluginManager*>(parent))
{
    Q_UNUSED(args)

    ClipPluginManager *clipPluginManager = static_cast<ClipPluginManager*>(parent);

    clipPluginManager->addSupportedMimetypes(QStringList() << "video/mpeg" << "video/mp4" << "video/mp2t");
}

VideoClipPlugin::~VideoClipPlugin()
{
    
}

AbstractProjectClip* VideoClipPlugin::createClip(const KUrl &url, ProjectFolder *parent) const
{
    VideoProjectClip *projectClip = new VideoProjectClip(url, parent, this);
    return projectClip;
}

AbstractProjectClip* VideoClipPlugin::loadClip(const QDomElement& description, ProjectFolder *parent) const
{
    VideoProjectClip *projectClip = new VideoProjectClip(description, parent, this);
    return projectClip;
}

TimelineClipItem* VideoClipPlugin::timelineClipView(AbstractTimelineClip* clip, QGraphicsItem* parent) const
{
    return new VideoTimelineClipItem(static_cast<VideoTimelineClip*>(clip), parent);
}

#include "videoclipplugin.moc"
