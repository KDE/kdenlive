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


/**
 * @class AbstractClipPlugin
 * @brief Abstract base class for clip plugins.
 * 
 * A clip plugin has to exist for every clip type. It should be created by the plugins factory (which
 * will be loaded on startup) and is responsible for providing means to open a clip of the implemented
 * type (for example by registering a mimetype to the add file dialog).
 * The plugin object is also responsible for creating project clip objects.
 */


class KDE_EXPORT AbstractClipPlugin : public QObject
{
    Q_OBJECT

public:
    /** @brief Should do basic setup and registering the different ways of creating a clip. */
    explicit AbstractClipPlugin(ClipPluginManager* parent);
    virtual ~AbstractClipPlugin();

    /** @brief Should return a clip created from @param url. */
    virtual AbstractProjectClip *createClip(const KUrl &url, ProjectFolder *parent) const = 0;
    /**
     * @brief Should return a clip created from @paramd description.
     * 
     * This function is only used when opening a project.
     */
    virtual AbstractProjectClip *loadClip(const QDomElement &description, ProjectFolder *parent) const = 0;

    /**
     * @brief Returns a timelineView clip item for the provided timeline clip.
     * @param clip timeline clip to create view for.
     */
    // change to QObject *timelineClipView(TimelineViewType type, AbstractTimelineClip *clip, QObject *parent) once we have different timeline views
    virtual TimelineClipItem *timelineClipView(AbstractTimelineClip *clip, QGraphicsItem* parent) const;

protected:
    ClipPluginManager *m_parent;
};

#endif
