/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "resizeclipcommand.h"
#include "core.h"
#include "project/projectmanager.h"
#include "project/project.h"
#include "project/producerwrapper.h"
#include "project/timeline.h"
#include "project/timelinetrack.h"
#include "project/abstracttimelineclip.h"
#include "project/abstractprojectclip.h"
#include <KLocalizedString>

#include <KDebug>


ResizeClipCommand::ResizeClipCommand(int track, int position, int in, int out, int oldIn, int oldOut, QUndoCommand* parent) :
    QUndoCommand(parent),
    m_track(track),
    m_position(position),
    m_in(in),
    m_out(out),
    m_oldIn(oldIn),
    m_oldOut(oldOut),
    m_firstTime(false)
{
    setText(i18n("Resize Clip"));

    if (parent) {
        redo();
        m_firstTime = true;
    }
}

void ResizeClipCommand::redo()
{
    if (m_firstTime) {
        m_firstTime = false;
    } else {
        resize(m_in, m_out);
    }
}

void ResizeClipCommand::undo()
{
    resize(m_oldIn, m_oldOut);
}

void ResizeClipCommand::resize(int in, int out)
{
    TimelineTrack *track = pCore->projectManager()->current()->timeline()->track(m_track);
    Mlt::Playlist *playlist = track->playlist();
    int index = playlist->get_clip_index_at(m_position);
    AbstractTimelineClip *clip = track->clip(index);

    if (in < 0) {
        if (!clip->projectClip()->hasLimitedDuration()) {
            out += -in;
        }
        in = 0;
    }

    int length = out - in + 1;
    int diff = length - clip->duration();
    if (!clip->projectClip()->hasLimitedDuration() && length > clip->producer()->get_length()) {
        clip->producer()->parent().set("length", length);
        clip->producer()->parent().set("out", length + clip->producer()->parent().get_in() - 1);
        clip->producer()->set("length", length);
    }

    // why does this not work/notifiy the playlist about duration changes??
//     clip->producer()->set_in_and_out(in, out);
    playlist->resize_clip(index, in, out);

    int nextIndex = index + 1;
    Mlt::Producer *next = playlist->get_clip(nextIndex);
    if (next) {
        if (next->is_blank()) {
            int blankLength = next->get_playtime() - diff;
            playlist->remove(nextIndex);
            if (blankLength == 0) {
                track->adjustIndices(clip, -1);
            } else {
                playlist->insert_blank(nextIndex, blankLength - 1);
            }
        } else if (diff < 0) {
            playlist->insert_blank(nextIndex, -diff - 1);
            track->adjustIndices(clip, 1);
        }
        delete next;
    }

    clip->emitResized();
}

