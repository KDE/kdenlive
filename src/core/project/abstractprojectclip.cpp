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
#include <mlt++/Mlt.h>
#include <QDomElement>


AbstractProjectClip::AbstractProjectClip(const KUrl& url, QObject* parent) :
    QObject(parent),
    m_url(url)
{
    if (url.isValid()) {
        m_name = url.fileName();
    }
}

AbstractProjectClip::AbstractProjectClip(ProducerWrapper* producer, QObject* parent) :
    QObject(parent),
    m_baseProducer(producer)
{
}

AbstractProjectClip::AbstractProjectClip(const QDomElement& description, QObject* parent) :
    QObject(parent)
{
    Q_ASSERT(description.hasAttribute("id"));

    m_id = description.attribute("id").toInt();
    m_url = KUrl(description.attribute("url"));
    m_name = description.attribute("name");
    m_description = description.attribute("description");
}


AbstractProjectClip::~AbstractProjectClip()
{
    // ?
    delete m_baseProducer;
}

int AbstractProjectClip::id() const
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

QString AbstractProjectClip::name() const
{
    return m_name;
}

void AbstractProjectClip::setName(const QString& name)
{
    m_name = name;
}

QString AbstractProjectClip::description() const
{
    return m_description;
}

void AbstractProjectClip::setDescription(const QString& description)
{
    m_description = description;
}

bool AbstractProjectClip::hasLimitedDuration() const
{
    return m_hasLimitedDuration;
}

int AbstractProjectClip::duration() const
{
    return m_baseProducer->get_playtime();
}

#include "abstractprojectclip.moc"
