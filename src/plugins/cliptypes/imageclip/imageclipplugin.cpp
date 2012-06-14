/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "imageclipplugin.h"
#include "imageprojectclip.h"
#include "imagetimelineclip.h"
#include "imagetimelineclipitem.h"
#include "core/project/clippluginmanager.h"
#include "core/project/projectfolder.h"
#include "core/project/abstracttimelineclip.h"
#include <KPluginFactory>
#include <QDomElement>

K_PLUGIN_FACTORY( ImageClipFactory, registerPlugin<ImageClipPlugin>(); )
K_EXPORT_PLUGIN( ImageClipFactory( "kdenliveimageclip" ) )

ImageClipPlugin::ImageClipPlugin(QObject* parent, const QVariantList& args) :
    AbstractClipPlugin(static_cast<ClipPluginManager*>(parent))
{
    Q_UNUSED(args)

    ClipPluginManager *clipPluginManager = static_cast<ClipPluginManager*>(parent);

    clipPluginManager->addSupportedMimetypes(QStringList() << "image/gif" << "image/jpeg" << "image/png" << "image/x-tga" << "image/x-bmp"
                                                           << "image/svg+xml" << "image/tiff" << "image/x-xcf" << "image/x-xcf-gimp"
                                                           << "image/x-vnd.adobe.photoshop" << "image/x-pcx" << "image/x-exr");
}

ImageClipPlugin::~ImageClipPlugin()
{
    
}

AbstractProjectClip* ImageClipPlugin::createClip(const KUrl &url, ProjectFolder *parent) const
{
    ImageProjectClip *projectClip = new ImageProjectClip(url, parent, this);
    return projectClip;
}

AbstractProjectClip* ImageClipPlugin::loadClip(const QDomElement& description, ProjectFolder *parent) const
{
    ImageProjectClip *projectClip = new ImageProjectClip(description, parent, this);
    return projectClip;
}

TimelineClipItem* ImageClipPlugin::timelineClipView(AbstractTimelineClip* clip, QGraphicsItem* parent) const
{
    return new ImageTimelineClipItem(static_cast<ImageTimelineClip*>(clip), parent);
}

#include "imageclipplugin.moc"
