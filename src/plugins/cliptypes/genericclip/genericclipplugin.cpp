/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "genericclipplugin.h"
#include "genericprojectclip.h"
#include "generictimelineclip.h"
#include "generictimelineclipitem.h"
#include "core/project/clippluginmanager.h"
#include "core/project/projectfolder.h"
#include "core/project/abstracttimelineclip.h"
#include <KPluginFactory>
#include <QDomElement>

K_PLUGIN_FACTORY( ColorClipFactory, registerPlugin<GenericClipPlugin>(); )
K_EXPORT_PLUGIN( ColorClipFactory( "kdenlivegenericclip" ) )

GenericClipPlugin::GenericClipPlugin(QObject* parent, const QVariantList& args) :
    AbstractClipPlugin(static_cast<ClipPluginManager*>(parent))
{
    Q_UNUSED(args)

    ClipPluginManager *clipPluginManager = static_cast<ClipPluginManager*>(parent);

}

GenericClipPlugin::~GenericClipPlugin()
{
    
}

AbstractProjectClip* GenericClipPlugin::createClip(const QString &service, Mlt::Properties props, const QString &id, ProjectFolder *parent) const
{
    GenericProjectClip *projectClip = new GenericProjectClip(service, props, id, parent, this);
    return projectClip;
}

AbstractProjectClip* GenericClipPlugin::createClip(const KUrl &url, const QString &id, ProjectFolder *parent) const
{
    GenericProjectClip *projectClip = new GenericProjectClip(url, id, parent, this);
    return projectClip;
}

AbstractProjectClip* GenericClipPlugin::loadClip(const QDomElement& description, ProjectFolder *parent) const
{
    GenericProjectClip *projectClip = new GenericProjectClip(description, parent, this);
    return projectClip;
}

QString GenericClipPlugin::nameForService(const QString &service) const
{
    // Name should be defined by MLT's yml data
    return service;
}

void GenericClipPlugin::fillDescription(Mlt::Properties properties, ProducerDescription *description)
{
}

TimelineClipItem* GenericClipPlugin::timelineClipView(AbstractTimelineClip* clip, QGraphicsItem* parent) const
{
    return new GenericTimelineClipItem(static_cast<GenericTimelineClip*>(clip), parent);
}

#include "genericclipplugin.moc"
