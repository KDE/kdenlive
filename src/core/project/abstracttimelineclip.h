/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTTIMELINECLIP_H
#define ABSTRACTTIMELINECLIP_H

#include "shiftingproducer.h"

class AbstractProjectClip;
class TimelineTrack;
class QUndoCommand;


/**
 * @class AbstractTimelineClip
 * @brief Base class for timeline clips (layer on top of cut producers).
 * 
 * 
 */


class KDE_EXPORT AbstractTimelineClip : public ShiftingProducer
{
    Q_OBJECT

public:
    /** 
     * @brief Creates a new timeline clip.
     * @param producer producer this timeline clip should handle (the cut producer/playlist entry, not the parent)
     * @param projectClip project clip this instance belongs to
     * @param parent track this clip is inside of; the constructor does not add the clip to the track, you have to do this manually
     */
    AbstractTimelineClip(ProducerWrapper *producer, AbstractProjectClip *projectClip, TimelineTrack *parent);
    virtual ~AbstractTimelineClip();

    /** @brief Returns the project clip this instance belongs to. */
    AbstractProjectClip *projectClip();
    /** @brief Returns the parent track. */
    TimelineTrack *track();
    /** @brief Returns the clips index in the parent mlt_playlist (blanks are counted too!). */
    int index() const;

    /** @brief Returns a pointer to the previous clip (timewise, per track) or NULL. */
    AbstractTimelineClip *before();
    /** @brief Returns a pointer to the next clip (timewise, per track) or NULL. */
    AbstractTimelineClip *after();

    /** @brief Returns the position of the clip (in frames). */
    int position() const;
    /** @brief Returns the duration of the clip (in frames). */
    int duration() const;
    /** @brief Returns the in point of the clip (in frames). */
    int in() const;
    /** @brief Returns the out point of the clip (in frames). */
    int out() const;

    /** @brief Returns the (displayable) name of the clip (same as AbstractProjectClip::name). */
    QString name() const;

    /** @brief Emits resized. */
    void emitResized();
    /** @brief Emits moved. */
    void emitMoved();

public slots:
    /**
     * @brief Moves the clip.
     * @param position new position
     * @param parentCommand command the MoveClipCommand should be parented to
     * 
     * If parentCommand is not supplied, the created command is pushed to the undo stack.
     * This function does not perform any safety checks! Make sure there is sufficient blank space
     * at @param position.
     * 
     * @see MoveClipCommand
     */
    void setPosition(int position, QUndoCommand *parentCommand = 0);

    /**
     * @brief Sets a new in point.
     * @param in new in point in frames
     * @param parentCommand command the ResizeClipCommand should be parented to
     * 
     * The clips postion will not be changed, but the absolute out point (position + duration).
     * 
     * @see setInOut
     */
    virtual void setIn(int in, QUndoCommand *parentCommand = 0);
    /**
     * @brief Sets a new out point.
     * @param out new out point in frames
     * @param parentCommand command the ResizeClipCommand should be parented to
     * 
     * @see setInOut
     */
    virtual void setOut(int out, QUndoCommand *parentCommand = 0);
    /**
     * @brief Sets new in and out points.
     * @param in new in point in frames
     * @param out new out point in frames
     * @param parentCommand command the ResizeClipCommand should be parented to
     * 
     * If parentCommand is not supplied, the created command is pushed to the undo stack.
     * This function does not perform any safety checks! Make sure the supplied in and out points are
     * valid for this clip and that there is sufficient blank space.
     * The clips postion will not be changed, but the absolute out point (position + duration).
     * 
     * @see ResizeClipCommand
     */
    virtual void setInOut(int in, int out, QUndoCommand *parentCommand = 0);

signals:
    void resized();
    void moved();

protected:
    AbstractProjectClip *m_projectClip;
    TimelineTrack *m_parent;
};

#endif
