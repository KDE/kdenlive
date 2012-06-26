/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "moveclipcommand.h"
#include "core.h"
#include "project/projectmanager.h"
#include "project/project.h"
#include "project/timeline.h"
#include "project/timelinetrack.h"
#include "project/abstracttimelineclip.h"
#include "project/producerwrapper.h"
#include <mlt++/Mlt.h>
#include <KLocalizedString>


MoveClipCommand::MoveClipCommand(int track, int position, int oldPosition, QUndoCommand* parent) :
    QUndoCommand(parent),
    m_track(track),
    m_position(position),
    m_oldPosition(oldPosition),
    m_firstTime(false)
{
    setText(i18n("Move Clip"));

    if (parent) {
        redo();
        m_firstTime = true;
    }
}

void MoveClipCommand::redo()
{
    if (m_firstTime) {
        m_firstTime = false;
    } else {
        move(m_position, m_oldPosition);
    }
}

void MoveClipCommand::undo()
{
    move(m_oldPosition, m_position);
}

void MoveClipCommand::move(int position, int oldPosition)
{
    TimelineTrack *track = pCore->projectManager()->current()->timeline()->track(m_track);
    Mlt::Playlist *playlist = track->playlist();
    int index = playlist->get_clip_index_at(oldPosition);
    AbstractTimelineClip *clip = track->clip(index);

    Mlt::Producer *producer = playlist->replace_with_blank(index);

    // necessary if clip is only moved by a duration shorter than its duration
    // in this case the blank created by replace_with_blank and the following one need to be joined
    playlist->consolidate_blanks();

    playlist->insert_at(position, producer, 1);
    clip->setProducer(new ProducerWrapper(playlist->get_clip_at(position)));
    delete producer;

    playlist->consolidate_blanks();

    track->setClipIndex(clip, playlist->get_clip_index_at(position));

    clip->emitMoved();
}
