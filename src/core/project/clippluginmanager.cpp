/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "clippluginmanager.h"
#include "abstractclipplugin.h"
#include "abstractprojectclip.h"
#include "producerwrapper.h"
#include "kdenlivesettings.h"
#include <mlt++/Mlt.h>
#include <KPluginLoader>
#include <KServiceTypeTrader>
#include <KPluginInfo>
#include <QFile>
#include <QDomElement>
#include <KDebug>


ClipPluginManager::ClipPluginManager(QObject* parent) :
    QObject(parent)
{
    KService::List availableClipServices = KServiceTypeTrader::self()->query("Kdenlive/Clip");
    for (int i = 0; i < availableClipServices.count(); ++i) {
        KPluginFactory *factory = KPluginLoader(availableClipServices.at(i)->library()).factory();
        KPluginInfo info = KPluginInfo(availableClipServices.at(i));
        if (factory && info.isValid()) {
            QStringList providedProducers = info.property("X-Kdenlive-ProvidedProducers").toStringList();
            AbstractClipPlugin *clipPlugin = factory->create<AbstractClipPlugin>(this);
            if (clipPlugin) {
                m_clipPlugins.append(clipPlugin);
                foreach (QString producer, providedProducers) {
                    kDebug() << producer;
                    m_clipPluginsForProducers.insert(producer, clipPlugin);
                }
            }
        }
    }
}

ClipPluginManager::~ClipPluginManager()
{
    qDeleteAll(m_clipPlugins);
}

AbstractProjectClip* ClipPluginManager::createClip(const KUrl& url) const
{
    if (QFile::exists(url.path())) {
        Mlt::Profile profile(KdenliveSettings::current_profile().toUtf8().constData());
        ProducerWrapper *producer = new ProducerWrapper(profile, url);
        QString producerType(producer->get("mlt_service"));
        if (m_clipPluginsForProducers.contains(producerType)) {
            AbstractProjectClip *clip = m_clipPluginsForProducers.value(producerType)->createClip(producer);
            return clip;
        } else {
            kWarning() << "no clip plugin available for mlt service " << producerType;
        }
        kDebug() << producer->get("mlt_service");
        // ?
        delete producer;
    } else {
        // TODO: proper warning
        kWarning() << url.path() << " does not exist";
    }
    return NULL;
}

AbstractProjectClip* ClipPluginManager::loadClip(const QDomElement& clipDescription) const
{
    QString producerType = clipDescription.attribute("producer_type");
    if (m_clipPluginsForProducers.contains(producerType)) {
        AbstractProjectClip *clip = m_clipPluginsForProducers.value(producerType)->loadClip(clipDescription);
        return clip;
    } else {
        kWarning() << "no clip plugin available for mlt service " << producerType;
        return NULL;
    }
}

#include "clippluginmanager.moc"
