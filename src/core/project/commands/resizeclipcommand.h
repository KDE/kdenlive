/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef RESIZECLIPCOMMAND_H
#define RESIZECLIPCOMMAND_H

#include <QUndoCommand>


/**
 * @class ResizeClipCommand
 * @brief Handles resizing a clip.
 * 
 * The clips position will not be changed. Modifying the in point will cause the absolute out point
 * to change (=position + duration).
 * 
 * WARNING: No safety checks are perfomed!
 * Make sure that the in and out points are valid for the clip and that there is sufficient white
 * space following the clip.
 */


class ResizeClipCommand : public QUndoCommand
{
public:
    /**
     * @brief Constructor; if no parent command is supplied redo is called (it won't be called again when pushing to the stack).
     * @param track index of the track containing the clip
     * @param position position of the clip
     * @param in new in point (in frames); negative values are allowed for clips without a duration
     *           limit and will cause "out" to be increased
     * @param out new out point
     * @param oldIn current in point
     * @param oldOut current out point
     */
    explicit ResizeClipCommand(int track, int position, int in, int out, int oldIn, int oldOut, QUndoCommand* parent = 0);

    void redo();
    void undo();

private:
    void resize(int in, int out);

    int m_track;
    int m_position;
    int m_in;
    int m_out;
    int m_oldIn;
    int m_oldOut;
    bool m_firstTime;
};

#endif
