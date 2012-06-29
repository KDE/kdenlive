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


/**
 * @class TimelineTrack
 * @brief Layer on top of Mlt::Playlist.
 */


class KDE_EXPORT TimelineTrack : public QObject
{
    Q_OBJECT

public:
    /** @brief Loads the track and the clips it contains. */
    TimelineTrack(ProducerWrapper *producer, Timeline* parent = 0);
    virtual ~TimelineTrack();

    /** @brief Returns a pointer to the timeline this track is a part of. */
    Timeline *timeline();
    /** @brief Returns a pointer to the track producer. */
    ProducerWrapper *producer();
    /** @brief Returns a pointer to the track playlist. */
    Mlt::Playlist *playlist();

    /** 
     * @brief Returns the index this track has inside the timeline. 
     *
     * Please note that this does not equal the index of its playlist since the track order in
     * Kdenlive is reversed.
     */
    int index() const;
//     int mltIndex() const;

    /** @brief Returns a list of pointers to the clips of this track in the order of their position. */
    QList<AbstractTimelineClip*> clips();
    /**
     * @brief Returns a pointer to the clip.
     * @param index index of the clip to return
     * 
     * The index is the one also used in the playlist so the counting includes blanks.
     */
    AbstractTimelineClip *clip(int index);
    /**
     * @brief Returns the index for a clip.
     * @param clip pointer to clip for which the index should be returned
     * 
     * @see function clip
     */
    int clipIndex(AbstractTimelineClip *clip) const;
    /** @brief Returns the position in frames of the supplied clip. */
    int clipPosition(const AbstractTimelineClip *clip) const;
    /** @brief Returns a pointer to the clip in front of the given one (timewise). */
    AbstractTimelineClip *before(AbstractTimelineClip *clip);
    /** @brief Returns a pointer to the clip following the given one (timewise). */
    AbstractTimelineClip *after(AbstractTimelineClip *clip);

    /**
     * @brief Adjusts the stored clip indices so that they match the ones used in the playlist again.
     * @param after if supplied only the indices for the clips following the given clip are adjusted
     * @param by amount to move indices by; if 0 the clips are automatically aligned with the entries in the playlist
     * 
     * Does not consider changes to the order! Only useful when blanks were inserted or removed.
     * @see adjustIndices
     */
    void adjustIndices(AbstractTimelineClip *after = 0, int by = 0);
    /**
     * @brief Assigns a new index to the clip
     * @param clip clip whose index should be changed
     * @param index new index for the clip
     * 
     * Indices of other clips are also adjusted in the process (but no changes to the order are
     * considered there).
     */
    void setClipIndex(AbstractTimelineClip *clip, int index);

    /** @brief Returns the (displayable) name. */
    QString name() const;
    /** @brief Sets the displayable name. */
    void setName(const QString &name);

    /** @brief Returns whether this track is hidden (=shows no video content). */
    bool isHidden() const;

    /** @brief Returns whether this track is muted. */
    bool isMute() const;

    /**
     * @brief Returns whether this track is locked.
     *
     * Does this really belong here? It's mainly GUI related...
     */
    bool isLocked() const;

    enum Types { VideoTrack, AudioTrack };

    /** @brief Returns the type of this track. */
    Types type() const;
    /**
     * @brief Sets the track type.
     *
     * WARNING: not yet implemented
     */
    void setType(Types type);

    /** @brief Emits nameChanged. */
    void emitNameChanged();
    /** @brief Emits visibilityChanged. */
    void emitVisibilityChanged();
    /** @brief Emits audibilityChanged. */
    void emitAudibilityChanged();
    /** @brief Emits lockStateChanged. */
    void emitLockStateChanged();

    /** @brief Emits durationChanged. */
    void emitDurationChanged();

    /** @brief Callback for the "producer-changed" event of the track producer to find out when the track duration changes. */
    static void producer_change(mlt_producer producer, TimelineTrack *self);

public slots:
    /** @brief Creates and pushes a ConfigureTrackCommand for changing the visibility. */
    void hide(bool hide);
    /** @brief Creates and pushes a ConfigureTrackCommand for changing the audibility. */
    void mute(bool mute);
    /** @brief Creates and pushes a ConfigureTrackCommand for changing the lock state. */
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
