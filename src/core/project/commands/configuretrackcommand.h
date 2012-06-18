/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef CONFIGURETRACKCOMMAND_H
#define CONFIGURETRACKCOMMAND_H

#include <QUndoCommand>
#include "project/timelinetrack.h"

typedef void (TimelineTrack::*TrackNotifier)();


class ConfigureTrackCommand : public QUndoCommand
{
public:
    explicit ConfigureTrackCommand(const char *text, int trackIndex, const QString &setting, const QString &value, const QString &oldValue, TrackNotifier notifier, QUndoCommand* parent = 0);

    void undo();
    void redo();

private:
    int m_trackIndex;
    QString m_setting;
    QString m_oldValue;
    QString m_value;
    TrackNotifier m_notifier;
};

#endif
