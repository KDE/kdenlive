/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MOVECLIPCOMMAND_H
#define MOVECLIPCOMMAND_H

#include <QUndoCommand>


/**
 * @class MoveClipCommand
 * @brief Handles changing a clips position inside a track.
 * 
 * WARNING: No safety checks are performed. Make sure there is sufficient white space at the new position.
 */


class MoveClipCommand : public QUndoCommand
{
public:
    /**
     * @brief Constructor; if no parent command is supplied redo is called (it won't be called again when pushing to the stack).
     * @param track index of the track containing the clip
     * @param position new position
     * @param oldPosition current/old position
     */
    explicit MoveClipCommand(int track, int position, int oldPosition, QUndoCommand* parent = 0);

    void redo();
    void undo();

private:
    void move(int position, int oldPosition);

    int m_track;
    int m_position;
    int m_oldPosition;
    bool m_firstTime;
};

#endif
