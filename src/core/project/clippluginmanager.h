/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef CLIPPLUGINMANAGER_H
#define CLIPPLUGINMANAGER_H

#include <kdemacros.h>
#include <QObject>
#include <QHash>

class AbstractClipPlugin;
class AbstractProjectClip;
class KUrl;
class QDomElement;


class KDE_EXPORT ClipPluginManager : public QObject
{
    Q_OBJECT

public:
    explicit ClipPluginManager(QObject* parent = 0);
    ~ClipPluginManager();

    AbstractProjectClip *createClip(const KUrl &url) const;
    AbstractProjectClip *loadClip(const QDomElement &clipDescription) const;

private:
    QList<AbstractClipPlugin *> m_clipPlugins;
    QHash<QString, AbstractClipPlugin*> m_clipPluginsForProducers;
};

#endif
