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


class ResizeClipCommand : public QUndoCommand
{
public:
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
