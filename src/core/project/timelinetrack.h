/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINETRACK_H
#define TIMELINETRACK_H

#include <mlt/framework/mlt_producer.h>
#include <QObject>
#include <QMap>
#include <kdemacros.h>

class Timeline;
class ProducerWrapper;
class EffectDevice;
class AbstractTimelineClip;
namespace Mlt
{
    class Playlist;
    class Event;
}


class KDE_EXPORT TimelineTrack : public QObject
{
    Q_OBJECT

public:
    TimelineTrack(ProducerWrapper *producer, Timeline* parent = 0);
    virtual ~TimelineTrack();

    Timeline *timeline();
    ProducerWrapper *producer();
    Mlt::Playlist *playlist();

    int index() const;
    int mltIndex() const;

    QList<AbstractTimelineClip*> clips();
    AbstractTimelineClip *clip(int index);
    int clipIndex(AbstractTimelineClip *clip) const;
    int clipPosition(const AbstractTimelineClip *clip) const;
    AbstractTimelineClip *before(AbstractTimelineClip *clip);
    AbstractTimelineClip *after(AbstractTimelineClip *clip);

    void adjustIndices(AbstractTimelineClip *after = 0, int by = 0);
    void setClipIndex(AbstractTimelineClip *clip, int index);

    QString name() const;
    void setName(const QString &name);

    bool isHidden() const;

    bool isMute() const;

    bool isLocked() const;

    enum Types { VideoTrack, AudioTrack };

    Types type() const;
    void setType(Types type);

    void emitNameChanged();
    void emitVisibilityChanged();
    void emitAudibilityChanged();
    void emitLockStateChanged();

    void emitDurationChanged();

    static void producer_change(mlt_producer producer, TimelineTrack *self);

public slots:
    void hide(bool hide);
    void mute(bool mute);
    void lock(bool lock);

signals:
    void nameChanged(const QString &name);
    void visibilityChanged(bool isHidden);
    void audibilityChanged(bool isMute);
    void lockStateChanged(bool isLocked);
    void durationChanged(int duration);

private:
    Timeline *m_parent;
    ProducerWrapper *m_producer;
    Mlt::Playlist *m_playlist;
    Mlt::Event *m_producerChangeEvent;
    EffectDevice *m_effectDevice;
    QMap<int, AbstractTimelineClip *> m_clips;
};

#endif
