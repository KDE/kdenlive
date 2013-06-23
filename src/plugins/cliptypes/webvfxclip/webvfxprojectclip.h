/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb.kdenlive.org>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef WEBVFXPROJECTCLIP_H
#define WEBVFXPROJECTCLIP_H

#include "core/project/abstractprojectclip.h"
#include <QPixmap>

class WebvfxClipPlugin;
class WebvfxTimelineClip;

namespace Mlt
{
    class Properties;
}

class WebvfxProjectClip : public AbstractProjectClip
{
    Q_OBJECT

public:
    WebvfxProjectClip(const KUrl& url, const QString &id, ProjectFolder* parent, WebvfxClipPlugin const *plugin);
    WebvfxProjectClip(const QString& service, Mlt::Properties props, const QString &id, ProjectFolder* parent, WebvfxClipPlugin const *plugin);
    WebvfxProjectClip(ProducerWrapper* producer, ProjectFolder* parent, WebvfxClipPlugin const *plugin);
    WebvfxProjectClip(const QDomElement &description, ProjectFolder *parent, WebvfxClipPlugin const *plugin);
    ~WebvfxProjectClip();

    AbstractTimelineClip *createInstance(TimelineTrack *parent, ProducerWrapper *producer = 0);
    void initProducer();
    void initProducer(const QString &service, Mlt::Properties props);
    void hash();

public slots:
    QPixmap thumbnail();

private:
    void init(int duration = 0);
    void parseScriptFile(const QString &url);

    QList<WebvfxTimelineClip *> m_instances;
};

#endif
