/*
Copyright (C) 2017  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "timelinefunctions.hpp"
#include "clipmodel.hpp"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"

#include <QDebug>
#include <klocalizedstring.h>

bool TimelineFunction::requestClipCut(std::shared_ptr<TimelineItemModel> timeline, int clipId, int position)
{
    int start = timeline->getClipPosition(clipId);
    int duration = timeline->getClipPlaytime(clipId);
    if (start > position || (start + duration) < position) {
        return false;
    }
    std::unordered_set<int> siblings = timeline->getGroupElements(clipId);
    if (siblings.size() > 0) {
        // TODO
    }
    int clipIn = timeline->getClipIn(clipId);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int in = position - start - clipIn;
    int out = start + duration - position;
    bool res = timeline->requestItemResize(clipId, position - start, true, true, undo, redo);
    int newId;
    const QString binId = timeline->getClipBinId(clipId);
    std::shared_ptr<EffectStackModel> sourceStack = timeline->getClipEffectStackModel(clipId);
    res = timeline->requestClipCreation(binId, in, out, newId, undo, redo);
    res = timeline->requestClipMove(newId, timeline->getItemTrackId(clipId), position, true, undo, redo);
    std::shared_ptr<EffectStackModel> destStack = timeline->getClipEffectStackModel(newId);
    destStack->importEffects(sourceStack);
    if (res) {
        pCore->pushUndo(undo, redo, i18n("Cut clip"));
    }
    return res;
}

int TimelineFunction::requestSpacerStartOperation(std::shared_ptr<TimelineItemModel> timeline, int trackId, int position)
{
    std::unordered_set<int> clips = timeline->getItemsAfterPosition(trackId, position, -1);
    if (clips.size() > 0) {
        timeline->requestClipsGroup(clips, false);
        return (*clips.cbegin());
    }
    return -1;
}

bool TimelineFunction::requestSpacerEndOperation(std::shared_ptr<TimelineItemModel> timeline, int clipId, int startPosition, int endPosition)
{
    // Move group back to original position
    int track = timeline->getItemTrackId(clipId);
    timeline->requestClipMove(clipId, track, startPosition, false, false);
    std::unordered_set<int> clips = timeline->getGroupElements(clipId);
    // break group
    timeline->requestClipUngroup(clipId, false);
    // Start undoable command
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int res = timeline->requestClipsGroup(clips, undo, redo);
    bool final = false;
    if (res > -1) {
        final = timeline->requestGroupMove(clipId, res, 0, endPosition - startPosition, true, undo, redo);
    }
    if (final) {
        final = timeline->requestClipUngroup(clipId, undo, redo);
    }
    if (final) {
        pCore->pushUndo(undo, redo, i18n("Insert space"));
        return true;
    }
    return false;
}
