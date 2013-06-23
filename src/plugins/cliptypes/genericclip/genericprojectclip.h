/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef GENERICPROJECTCLIP_H
#define GENERICPROJECTCLIP_H

#include "core/project/abstractprojectclip.h"
#include <QPixmap>

class GenericClipPlugin;
class GenericTimelineClip;

namespace Mlt
{
    class Properties;
}

class GenericProjectClip : public AbstractProjectClip
{
    Q_OBJECT

public:
    GenericProjectClip(const QString& service, Mlt::Properties props, const QString &id, ProjectFolder* parent, GenericClipPlugin const *plugin);
    GenericProjectClip(const KUrl& url, const QString &id, ProjectFolder* parent, GenericClipPlugin const *plugin);
    GenericProjectClip(ProducerWrapper* producer, ProjectFolder* parent, GenericClipPlugin const *plugin);
    GenericProjectClip(const QDomElement &description, ProjectFolder *parent, GenericClipPlugin const *plugin);
    ~GenericProjectClip();

    AbstractTimelineClip *createInstance(TimelineTrack *parent, ProducerWrapper *producer = 0);
    void initProducer();
    void initProducer(const QString &service, Mlt::Properties props);
    void hash();

public slots:
    QPixmap thumbnail();

private:
    void init(int duration = 0, int in = 0, int out = 0);

    QList<GenericTimelineClip *> m_instances;
};

#endif
