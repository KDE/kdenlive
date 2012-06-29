/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <KPluginInfo>
#include <QObject>
#include <QList>

class KPluginFactory;

/**
 * @class PluginManager
 * @brief Locates the plugins and allows to load them.
 */

class PluginManager : public QObject
{
    Q_OBJECT

public:
    /** @brief Locates the plugins. */
    explicit PluginManager(QObject* parent = 0);

    /** @brief Creates all plugins with should be auto loaded.
     * @see kdenliveplugin.desktop */
    void load();

private:
    QList<KPluginFactory*> m_factories;
    QList<KPluginInfo> m_infos;
};

#endif
