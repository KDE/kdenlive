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

#include <kdemacros.h>
#include <KUrl>

class ProducerWrapper;
class TimelineTrack;
class AbstractTimelineClip;
class QDomElement;


class KDE_EXPORT AbstractProjectClip : public QObject
{
    Q_OBJECT

public:
    AbstractProjectClip(const KUrl &url, QObject *parent = 0);
    AbstractProjectClip(ProducerWrapper *producer, QObject *parent = 0);
    AbstractProjectClip(const QDomElement &description, QObject *parent = 0);
    virtual ~AbstractProjectClip();

    virtual AbstractTimelineClip *addInstance(ProducerWrapper *producer, TimelineTrack *parent) = 0;

    int id() const;
    bool hasUrl() const;
    KUrl url() const;
    QString name() const;
    virtual void setName(const QString &name);
    QString description() const;
    virtual void setDescription(const QString &description);
    virtual bool hasLimitedDuration() const;
    virtual int duration() const;

protected:
    ProducerWrapper *m_baseProducer;
    int m_id;
    KUrl m_url;
    QString m_name;
    QString m_description;
    bool m_hasLimitedDuration;
};

#endif
