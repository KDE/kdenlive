/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTPROJECTCLIP_H
#define ABSTRACTPROJECTCLIP_H

#include "abstractprojectitem.h"
#include <KUrl>

class ProjectFolder;
class ProducerWrapper;
class TimelineTrack;
class AbstractTimelineClip;
class AbstractClipPlugin;
class QDomElement;


class KDE_EXPORT AbstractProjectClip : public AbstractProjectItem
{
    Q_OBJECT

public:
    AbstractProjectClip(const KUrl &url, ProjectFolder *parent, AbstractClipPlugin const *plugin);
    AbstractProjectClip(ProducerWrapper *producer, ProjectFolder *parent, AbstractClipPlugin const *plugin);
    AbstractProjectClip(const QDomElement &description, ProjectFolder *parent, AbstractClipPlugin const *plugin);
    virtual ~AbstractProjectClip();

    AbstractClipPlugin const *plugin() const;

    virtual AbstractTimelineClip *addInstance(ProducerWrapper *producer, TimelineTrack *parent) = 0;

    AbstractProjectClip *clip(int id);

    int id() const;
    bool hasUrl() const;
    KUrl url() const;
    virtual bool hasLimitedDuration() const;
    virtual int duration() const;

    virtual void setCurrent(bool current);

protected:
    AbstractClipPlugin const *m_plugin;
    ProducerWrapper *m_baseProducer;
    int m_id;
    KUrl m_url;
    bool m_hasLimitedDuration;
};

#endif
