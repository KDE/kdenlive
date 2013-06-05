/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "configuretrackcommand.h"
#include "core.h"
#include "project/projectmanager.h"
#include "project/project.h"
#include "project/timeline.h"
#include "project/producerwrapper.h"
#include <KLocalizedString>


ConfigureTrackCommand::ConfigureTrackCommand(const char *text, int trackIndex, const QString& setting, const QString& value, const QString &oldValue, TrackNotifier notifier, QUndoCommand* parent):
    QUndoCommand(parent),
    m_trackIndex(trackIndex),
    m_setting(setting),
    m_value(value),
    m_oldValue(oldValue),
    m_notifier(notifier)
{
    setText(i18n(text));
}

void ConfigureTrackCommand::redo()
{
    TimelineTrack *track = pCore->projectManager()->current()->timeline()->track(m_trackIndex);
    track->producer()->setProperty(m_setting, m_value);
    (track->*m_notifier)();
}


void ConfigureTrackCommand::undo()
{
    TimelineTrack *track = pCore->projectManager()->current()->timeline()->track(m_trackIndex);
    track->producer()->setProperty(m_setting, m_oldValue);
    (track->*m_notifier)();
}
