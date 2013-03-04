/*
Copyright (C) 2013  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "addremovetimelineclipcommand.h"
#include "core.h"
#include "project/producerwrapper.h"
#include "project/projectmanager.h"
#include "project/project.h"
#include "project/binmodel.h"
#include "project/abstractprojectclip.h"
#include "project/timeline.h"
#include "project/timelinetrack.h"
#include "project/abstracttimelineclip.h"
#include <mlt++/Mlt.h>
#include <KLocalizedString>


AddRemoveTimelineClipCommand::AddRemoveTimelineClipCommand(const QString& id, int track, int position, bool isAddCommand, int in, int out, QUndoCommand* parent) :
    QUndoCommand(parent),
    m_id(id),
    m_track(track),
    m_position(position),
    m_in(in),
    m_out(out),
    m_isAddCommand(isAddCommand)
{
    if (m_isAddCommand) {
        setText(i18n("Add clip to timeline"));
    } else {
        setText(i18n("Remove clip from timeline"));
    }
}

void AddRemoveTimelineClipCommand::undo()
{
    if (m_isAddCommand) {
        removeClip();
    } else {
        addClip();
    }
}

void AddRemoveTimelineClipCommand::redo()
{
    if (m_isAddCommand) {
        addClip();
    } else {
        removeClip();
    }
}

void AddRemoveTimelineClipCommand::addClip()
{
    AbstractProjectClip *projectClip = pCore->projectManager()->current()->bin()->clip(m_id);
    TimelineTrack *track = pCore->projectManager()->current()->timeline()->track(m_track);
    Mlt::Playlist *playlist = track->playlist();

    Q_ASSERT(projectClip && track);

    AbstractTimelineClip *clip = projectClip->createInstance(track);

    clip->producer()->set_in_and_out(m_in, m_out);

    playlist->insert_at(m_position, static_cast<Mlt::Producer*>(clip->producer()), 1);

    clip->setProducer(new ProducerWrapper(playlist->get_clip_at(m_position)));

    playlist->consolidate_blanks();

    track->setClipIndex(clip, playlist->get_clip_index_at(m_position));
}

void AddRemoveTimelineClipCommand::removeClip()
{
    
}
