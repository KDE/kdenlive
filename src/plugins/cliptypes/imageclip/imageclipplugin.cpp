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
#include "core/project/clippluginmanager.h"
#include "core/project/producerwrapper.h"
#include <KPluginFactory>
#include <QDomElement>

K_PLUGIN_FACTORY( ImageClipFactory, registerPlugin<ImageClipPlugin>(); )
K_EXPORT_PLUGIN( ImageClipFactory( "kdenliveimageclip" ) )

ImageClipPlugin::ImageClipPlugin(QObject* parent, const QVariantList& args) :
    AbstractClipPlugin(static_cast<ClipPluginManager*>(parent))
{
    Q_UNUSED(args)
}

ImageClipPlugin::~ImageClipPlugin()
{
    
}

AbstractProjectClip* ImageClipPlugin::createClip(ProducerWrapper *producer) const
{
    ImageProjectClip *projectClip = new ImageProjectClip(producer);
    return projectClip;
}

AbstractProjectClip* ImageClipPlugin::loadClip(const QDomElement& description, AbstractProjectItem *parent) const
{
    ImageProjectClip *projectClip = new ImageProjectClip(description, parent);
    return projectClip;
}

#include "imageclipplugin.moc"
