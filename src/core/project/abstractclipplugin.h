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

class TimelineClipItem;
class ProjectFolder;
class ClipPluginManager;
class AbstractProjectClip;
class AbstractTimelineClip;
class KUrl;
class QDomElement;
class QGraphicsItem;


class KDE_EXPORT AbstractClipPlugin : public QObject
{
    Q_OBJECT

public:
    explicit AbstractClipPlugin(ClipPluginManager* parent);
    virtual ~AbstractClipPlugin();

    virtual AbstractProjectClip *createClip(const KUrl &url, ProjectFolder *parent) const = 0;
    virtual AbstractProjectClip *loadClip(const QDomElement &description, ProjectFolder *parent) const = 0;

    // change to QObject *timelineClipView(TimelineViewType type, AbstractTimelineClip *clip, QObject *parent) once we have different timeline views
    virtual TimelineClipItem *timelineClipView(AbstractTimelineClip *clip, QGraphicsItem* parent) const;

protected:
    ClipPluginManager *m_parent;
};

#endif
