/*
Copyright (C) 2013  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ADDREMOVETIMELINECLIPCOMMAND_H
#define ADDREMOVETIMELINECLIPCOMMAND_H

#include <QUndoCommand>


/**
 * @class AddRemoveTimelineClipCommand
 * @brief Handles adding or removing a clip instance to the timeline.
 * 
 * WARNING: No safety checks are performed. Make sure there is sufficient white space at the given position.
 */

class AddRemoveTimelineClipCommand : public QUndoCommand
{
public:
    /**
     * @brief Constructor.
     * 
     * 
     */
    explicit AddRemoveTimelineClipCommand(const QString &id, int track, int position, bool isAddCommand = true, int in = 0, int out = -1, QUndoCommand* parent = 0);

    void undo();
    void redo();

private:
    void addClip();
    void removeClip();

    QString m_id;
    int m_track;
    int m_position;
    int m_in;
    int m_out;
    bool m_isAddCommand;
};

#endif
