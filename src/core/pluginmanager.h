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


class PluginManager : public QObject
{
    Q_OBJECT

public:
    explicit PluginManager(QObject* parent = 0);

    void load();

private:
    QList<KPluginFactory*> m_factories;
    QList<KPluginInfo> m_infos;
};

#endif
