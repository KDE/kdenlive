/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef GENERICPROJECTCLIPPLUGIN_H
#define GENERICPROJECTCLIPPLUGIN_H

#include "core/project/abstractclipplugin.h"
#include <kdemacros.h>
#include <QVariantList>


class KDE_EXPORT GenericClipPlugin : public AbstractClipPlugin
{
    Q_OBJECT

public:
    explicit GenericClipPlugin(QObject *parent, const QVariantList &args);
    ~GenericClipPlugin();

    AbstractProjectClip* createClip(const QString &service, Mlt::Properties props, const QString &id, ProjectFolder *parent) const;
    AbstractProjectClip *createClip(const KUrl &url, const QString &id, ProjectFolder *parent) const;
    AbstractProjectClip *loadClip(const QDomElement &description, ProjectFolder *parent) const;

    TimelineClipItem *timelineClipView(AbstractTimelineClip *clip, QGraphicsItem* parent) const;
    QString nameForService(const QString &) const;
};

#endif
