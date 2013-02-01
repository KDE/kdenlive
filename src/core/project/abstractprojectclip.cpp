/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstractprojectclip.h"
#include "producerwrapper.h"
#include "projectfolder.h"
#include "project.h"
#include "binmodel.h"
#include "monitor/monitormodel.h"
#include <mlt++/Mlt.h>
#include <QDomElement>


AbstractProjectClip::AbstractProjectClip(const KUrl& url, const QString &id, ProjectFolder* parent, AbstractClipPlugin const *plugin) :
    AbstractProjectItem(parent)
    , m_plugin(plugin)
    , m_url(url)
    , m_id(id)
{
    if (url.isValid()) {
        m_name = url.fileName();
    }
}

AbstractProjectClip::AbstractProjectClip(ProducerWrapper* producer, ProjectFolder* parent, AbstractClipPlugin const *plugin) :
    AbstractProjectItem(parent),
    m_plugin(plugin),
    m_baseProducer(producer)
{
    char *resource = producer->get("resource");
    if (resource) {
        m_url = KUrl(resource);
        if (m_url.isValid()) {
            m_name = m_url.fileName();
        }
    }
}

AbstractProjectClip::AbstractProjectClip(const QDomElement& description, ProjectFolder* parent, AbstractClipPlugin const *plugin) :
    AbstractProjectItem(description, parent),
    m_plugin(plugin)
{
    Q_ASSERT(description.hasAttribute("id"));

    m_id = description.attribute("id");
    m_url = KUrl(description.attribute("url"));
}


AbstractProjectClip::~AbstractProjectClip()
{
    // ?
    delete m_baseProducer;
}

AbstractClipPlugin const *AbstractProjectClip::plugin() const
{
    return m_plugin;
}

AbstractProjectClip* AbstractProjectClip::clip(const QString &id)
{
    if (id == m_id) {
        return this;
    }
    return NULL;
}

QString AbstractProjectClip::id() const
{
    return m_id;
}

bool AbstractProjectClip::hasUrl() const
{
    return m_url.isValid();
}

KUrl AbstractProjectClip::url() const
{
    return m_url;
}

bool AbstractProjectClip::hasLimitedDuration() const
{
    return m_hasLimitedDuration;
}

int AbstractProjectClip::duration() const
{
    return m_baseProducer->get_playtime();
}

void AbstractProjectClip::setCurrent(bool current)
{
    AbstractProjectItem::setCurrent(current);
    if (current) {
	initProducer();
        bin()->monitor()->setProducer(m_baseProducer);
    }
}

QDomElement AbstractProjectClip::toXml(QDomDocument& document) const
{
    QDomElement clip = document.createElement("clip");
    clip.setAttribute("id", id());
    clip.setAttribute("producer_type",m_baseProducer->get("mlt_service"));
    clip.setAttribute("name", name());
    if (!description().isEmpty()) {
        clip.setAttribute("description", description());
    }
    if (!m_url.isEmpty()) {
        clip.setAttribute("url", m_url.path());
    }
    return clip;
}

#include "abstractprojectclip.moc"
