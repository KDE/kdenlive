/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef VIDEOPROJECTCLIP_H
#define VIDEOPROJECTCLIP_H

#include "core/project/abstractprojectclip.h"
#include <QPixmap>

class VideoClipPlugin;
class VideoTimelineClip;


class VideoProjectClip : public AbstractProjectClip
{
    Q_OBJECT

public:
    VideoProjectClip(const KUrl& url, const QString &id, ProjectFolder* parent, VideoClipPlugin const *plugin);
    VideoProjectClip(ProducerWrapper* producer, ProjectFolder* parent, VideoClipPlugin const *plugin);
    VideoProjectClip(const QDomElement &description, ProjectFolder *parent, VideoClipPlugin const *plugin);
    ~VideoProjectClip();

    AbstractTimelineClip *addInstance(ProducerWrapper *producer, TimelineTrack *parent);
    void initProducer();
    void hash();

    QPixmap thumbnail();

private:
    void init();
    QList<VideoTimelineClip *> m_instances;
};

#endif
