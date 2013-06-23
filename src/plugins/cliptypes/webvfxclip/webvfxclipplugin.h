/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb.kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef WEBVFXPROJECTCLIPPLUGIN_H
#define WEBVFXPROJECTCLIPPLUGIN_H

#include "core/project/abstractclipplugin.h"
#include <kdemacros.h>
#include <QVariantList>


class KDE_EXPORT WebvfxClipPlugin : public AbstractClipPlugin
{
    Q_OBJECT

public:
    explicit WebvfxClipPlugin(QObject *parent, const QVariantList &args);
    ~WebvfxClipPlugin();

    AbstractProjectClip *createClip(const KUrl &url, const QString &id, ProjectFolder *parent) const;
    AbstractProjectClip *createClip(const QString &service, Mlt::Properties props, const QString &id, ProjectFolder *parent) const;
    AbstractProjectClip *loadClip(const QDomElement &description, ProjectFolder *parent) const;

    TimelineClipItem *timelineClipView(AbstractTimelineClip *clip, QGraphicsItem* parent) const;
    QString nameForService(const QString &) const;
    ProducerDescription *fillDescription(Mlt::Properties properties, ProducerDescription *description);
    QDomElement createDescription(Mlt::Properties properties);
    bool requiresClipReload(const QString &property);
};

#endif
