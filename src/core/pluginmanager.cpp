/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "pluginmanager.h"
#include <KServiceTypeTrader>


PluginManager::PluginManager(QObject* parent) :
    QObject(parent)
{
    KService::List plugins = KServiceTypeTrader::self()->query("Kdenlive/Plugin");
    for (int i = 0; i < plugins.count(); ++i) {
        KPluginFactory *factory = KPluginLoader(plugins.at(i)->library()).factory();
        KPluginInfo info = KPluginInfo(plugins.at(i));
        if (factory && info.isValid()) {
            m_factories.append(factory);
            m_infos.append(info);
        }
    }
}

void PluginManager::load()
{
    for (int i = 0; i < m_factories.count(); ++i) {
        KPluginInfo info = m_infos.at(i);
        if (info.property("X-Kdenlive-Autoload").toBool()) {
            m_factories.at(i)->create<QObject>(this);
        }
    }
}

#include "pluginmanager.moc"
