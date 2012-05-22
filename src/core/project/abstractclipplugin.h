/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTPROJECTCLIPPLUGIN_H
#define ABSTRACTPROJECTCLIPPLUGIN_H

#include <kdemacros.h>
#include <QObject>

class ClipPluginManager;
class AbstractProjectClip;
class ProducerWrapper;
class QDomElement;


class KDE_EXPORT AbstractClipPlugin : public QObject
{
    Q_OBJECT

public:
    explicit AbstractClipPlugin(ClipPluginManager* parent = 0);
    virtual ~AbstractClipPlugin();

    virtual AbstractProjectClip *createClip(ProducerWrapper *producer) const = 0;
    virtual AbstractProjectClip *loadClip(const QDomElement &description) const = 0;

protected:
    ClipPluginManager *m_parent;
};

#endif
