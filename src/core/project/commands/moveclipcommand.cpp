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
#include <mlt++/Mlt.h>
#include <KLocalizedString>

#include <KDebug>


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

    int indexAtPosition = playlist->get_clip_index_at(position);

    if (indexAtPosition == index) {
        // insert blank before clip
        playlist->insert_blank(index, position - oldPosition - 1);
        if (index + 2 < playlist->count()) {
            // remove same amount after clip
            playlist->remove_region(position + clip->duration(), position - oldPosition);
        }
    } else {
        Q_ASSERT(playlist->is_blank(indexAtPosition));

        int oldCount = playlist->count();
        // we cannot just use the clip's duration as the region then might overlap with the clip at its current position
        int amountToRemove = qMin(clip->duration(), oldPosition - position);
        indexAtPosition = playlist->remove_region(position, amountToRemove);
        if (index > indexAtPosition) {
            // The index needs to be adjusted when a whole blank was removed or one was splitted
            index += playlist->count() - oldCount;
        }
        playlist->move(index, indexAtPosition);

        if (index >= indexAtPosition) {
            // clip was moved to a position smaller than the old one, so the indices shifted
            ++index;
        }
        playlist->insert_blank(index, amountToRemove - 1);
    }

    playlist->consolidate_blanks();

    track->adjustIndices();

    clip->emitMoved();
}
