/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb.kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "webvfxclipplugin.h"
#include "webvfxprojectclip.h"
#include "webvfxtimelineclip.h"
#include "webvfxtimelineclipitem.h"
#include "core/project/clippluginmanager.h"
#include "core/producersystem/producerdescription.h"
#include "core/project/projectfolder.h"
#include "core/project/abstracttimelineclip.h"
#include "core/core.h"
#include <KPluginFactory>
#include <KLocale>
#include <QDomElement>
#include <QFile>
#include <KDebug>

K_PLUGIN_FACTORY( WebvfxClipFactory, registerPlugin<WebvfxClipPlugin>(); )
K_EXPORT_PLUGIN( WebvfxClipFactory( "kdenlivewebvfxclip" ) )

WebvfxClipPlugin::WebvfxClipPlugin(QObject* parent, const QVariantList& args) :
    AbstractClipPlugin(static_cast<ClipPluginManager*>(parent))
{
    Q_UNUSED(args)

    ClipPluginManager *clipPluginManager = static_cast<ClipPluginManager*>(parent);

    clipPluginManager->addSupportedMimetypes(QStringList() << QLatin1String("text/html") << QLatin1String("text/x-qml"));
}

WebvfxClipPlugin::~WebvfxClipPlugin()
{
    
}

AbstractProjectClip* WebvfxClipPlugin::createClip(const KUrl &url, const QString &id, ProjectFolder *parent) const
{
    WebvfxProjectClip *projectClip = new WebvfxProjectClip(url, id, parent, this);
    return projectClip;
}

AbstractProjectClip* WebvfxClipPlugin::createClip(const QString &service, Mlt::Properties props, const QString &id, ProjectFolder *parent) const
{
    WebvfxProjectClip *projectClip = new WebvfxProjectClip(service, props, id, parent, this);
    return projectClip;
}

AbstractProjectClip* WebvfxClipPlugin::loadClip(const QDomElement& description, ProjectFolder *parent) const
{
    WebvfxProjectClip *projectClip = new WebvfxProjectClip(description, parent, this);
    return projectClip;
}

QString WebvfxClipPlugin::nameForService(const QString &) const
{
    return i18n("Webvfx Clip");
}

ProducerDescription *WebvfxClipPlugin::fillDescription(Mlt::Properties properties, ProducerDescription *description)
{
    QString url = properties.get("resource");
    QDomDocument doc;
    QFile file(url);
    doc.setContent(&file, false);
    file.close();
    QDomNodeList params = doc.documentElement().elementsByTagName("metainfo");
    if (params.isEmpty()) return NULL;
    if (!description) {
        description = new ProducerDescription(params.at(0).toElement(), 1, pCore->producerRepository());
        return description;
    }
    return NULL;
}

bool WebvfxClipPlugin::requiresClipReload(const QString &property)
{
    // Webvfx needs a reload for each property change.
    return false;
}

TimelineClipItem* WebvfxClipPlugin::timelineClipView(AbstractTimelineClip* clip, QGraphicsItem* parent) const
{
    return new WebvfxTimelineClipItem(static_cast<WebvfxTimelineClip*>(clip), parent);
}

#include "webvfxclipplugin.moc"
