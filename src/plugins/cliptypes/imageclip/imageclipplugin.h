/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef IMAGEPROJECTCLIPPLUGIN_H
#define IMAGEPROJECTCLIPPLUGIN_H

#include "core/project/abstractclipplugin.h"
#include <kdemacros.h>
#include <QVariantList>


class KDE_EXPORT ImageClipPlugin : public AbstractClipPlugin
{
    Q_OBJECT

public:
    explicit ImageClipPlugin(QObject *parent, const QVariantList &args);
    ~ImageClipPlugin();

    AbstractProjectClip *createClip(const KUrl &url, ProjectFolder *parent) const;
    AbstractProjectClip *loadClip(const QDomElement &description, ProjectFolder *parent) const;

    TimelineClipItem *timelineClipView(AbstractTimelineClip *clip, QGraphicsItem* parent) const;
};

#endif
