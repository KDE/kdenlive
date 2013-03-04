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


/**
 * @class AbstractProjectClip
 * @brief Represents a clip in the project (not timeline).
 * 
 * Has to be the base class for every clip type implementation.
 * It is created by it's plugin object and is in return responsible for creating timeline instances.
 */


class KDE_EXPORT AbstractProjectClip : public AbstractProjectItem
{
    Q_OBJECT

public:
    /** @brief Constructor.
     * @param url url of the file this clip is describing; the filename is used as the item's name
     * @param parent parent item/folder
     * @param plugin plugin ...
     */
    AbstractProjectClip(const KUrl &url, const QString &id, ProjectFolder *parent, AbstractClipPlugin const *plugin);
    /**
     * @brief Constructor; tries to get url and name from the producer's resource.
     */
    AbstractProjectClip(ProducerWrapper *producer, ProjectFolder *parent, AbstractClipPlugin const *plugin);
    /**
     * @brief Constructor used when opening a project.
     * @param description element describing the clip; the attributes "id" and "url" are used
     */
    AbstractProjectClip(const QDomElement &description, ProjectFolder *parent, AbstractClipPlugin const *plugin);
    virtual ~AbstractProjectClip();

    /** @brief Returns a pointer to the clip's plugin. */
    AbstractClipPlugin const *plugin() const;

    /** @brief Should return a timeline clip/instance of this clip. */
    virtual AbstractTimelineClip *createInstance(TimelineTrack *parent, ProducerWrapper *producer = 0) = 0;
    
    /** @brief Create the producer on demand if it was not already loaded. */
    virtual void initProducer() = 0;
    
    /** @brief Returns a unique hash identifier used to store clip thumbnails. */
    virtual void hash() = 0;

    /** @brief Returns this if @param id matches the clip's id or NULL otherwise. */
    AbstractProjectClip *clip(const QString &id);

    /** @brief Returns the clip's id. */
    QString id() const;
    /** @brief Returns whether this clip has a url (=describes a file) or not. */
    bool hasUrl() const;
    /** @brief Returns the clip's url. */
    KUrl url() const;

    /** @brief Returns whether this clip has a limited duration or whether it is resizable ad infinitum. */
    virtual bool hasLimitedDuration() const;
    /** @brief Returns the clip's duration. */
    virtual int duration() const;

    /** @brief Calls AbstractProjectItem::setCurrent and sets the bin monitor to use the clip's producer. */
    virtual void setCurrent(bool current);

    virtual QDomElement toXml(QDomDocument &document) const;

    /** @brief Returns the "base" producer from which the entries / "cut" producers in the timeline are created. */
    virtual ProducerWrapper *timelineBaseProducer();
    
protected:
    AbstractClipPlugin const *m_plugin;
    ProducerWrapper *m_baseProducer;
    QString m_id;
    KUrl m_url;
    bool m_hasLimitedDuration;
    qint64 m_fileSize;
    QByteArray m_hash;
    
    /** @brief Returns a rounded border pixmap from the @param source pixmap. */
    QPixmap roundedPixmap(QPixmap source);

    /**
     * @brief Sets the "base" producer from which the entries / "cut" producers in the timeline are created.
     * @param producer pointer to the producer
     * 
     * Should be called in from a subclasses' createInstance function to so that we know about an already existing base producer.
     */
    virtual void setTimelineBaseProducer(ProducerWrapper *producer);

private:
    ProducerWrapper *m_timelineBaseProducer;
};

#endif
