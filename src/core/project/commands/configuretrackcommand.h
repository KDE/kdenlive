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

/** Pointer to a member function of TimelineTrack with no arguments. */
typedef void (TimelineTrack::*TrackNotifier)();


/**
 * @class ConfigureTrackCommand
 * @brief Handles the modification of track settings.
 */


class ConfigureTrackCommand : public QUndoCommand
{
public:
    /**
     * @brief Constructor.
     * @param text untranslated text describing the command (@see QUndoCommand::setText)
     * @param trackIndex index of the track
     * @param setting producer property name of the setting to be modified
     * @param value new value
     * @param oldValue current/old value
     * @param notifier notifier which should be called upon setting change
     */
    explicit ConfigureTrackCommand(const QString &text, int trackIndex, const QString &setting, const QString &value, const QString &oldValue, TrackNotifier notifier, QUndoCommand* parent = 0);

    void undo();
    void redo();

private:
    int m_trackIndex;
    QString m_setting;
    QString m_value;
    QString m_oldValue;
    TrackNotifier m_notifier;
};

#endif
