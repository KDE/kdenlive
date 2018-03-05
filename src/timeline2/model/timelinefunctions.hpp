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

#ifndef TIMELINEFUNCTIONS_H
#define TIMELINEFUNCTIONS_H

#include "definitions.h"
#include "undohelper.hpp"
#include <memory>
#include <unordered_set>

/**
 * @namespace TimelineFunction
 * @brief This namespace contains a list of static methods for advanced timeline editing features
 *  based on timelinemodel methods
 */

class TimelineItemModel;
struct TimelineFunctions
{
    /* @brief Cuts a clip at given position
       If the clip is part of the group, all clips of the groups are cut at the same position. The group structure is then preserved for clips on both sides
       Returns true on success
       @param timeline : ptr to the timeline model
       @param clipId: Id of the clip to split
       @param position: position (in frames) where to cut
    */
    static bool requestClipCut(std::shared_ptr<TimelineItemModel> timeline, int clipId, int position);
    /* This is the same function, except that it accumulates undo/redo */
    static bool requestClipCut(std::shared_ptr<TimelineItemModel> timeline, int clipId, int position, Fun &undo, Fun &redo);
    /* This is the same function, except that it accumulates undo/redo and do not deal with groups. Do not call directly */
    static bool processClipCut(std::shared_ptr<TimelineItemModel> timeline, int clipId, int position, int &newId, Fun &undo, Fun &redo);

    /* @brief Makes a perfect copy of a given clip, but do not insert it */
    static bool copyClip(std::shared_ptr<TimelineItemModel> timeline, int clipId, int &newId, PlaylistState::ClipState state, Fun &undo, Fun &redo);

    /* @brief Request the addition of multiple clips to the timeline
     * If the addition of any of the clips fails, the entire operation is undone.
     * @returns true on success, false otherwise.
     * @param binIds the list of bin ids to be inserted
     * @param trackId the track where the insertion should happen
     * @param position the position at which the clips should be inserted
     * @param clipIds a return parameter with the ids assigned to the clips if success, empty otherwise
    */
    static bool requestMultipleClipsInsertion(std::shared_ptr<TimelineItemModel> timeline, const QStringList &binIds, int trackId, int position, QList<int> &clipIds, bool logUndo, bool refreshView);

    static int requestSpacerStartOperation(std::shared_ptr<TimelineItemModel> timeline, int trackId, int position);
    static bool requestSpacerEndOperation(std::shared_ptr<TimelineItemModel> timeline, int clipId, int startPosition, int endPosition);
    static bool extractZone(std::shared_ptr<TimelineItemModel> timeline, QVector<int> tracks, QPoint zone, bool liftOnly);
    static bool liftZone(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo);
    static bool removeSpace(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo);
    static bool insertSpace(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo);
    static bool insertZone(std::shared_ptr<TimelineItemModel> timeline, int trackId, const QString &binId, int insertFrame, QPoint zone, bool overwrite);

    static bool requestItemCopy(std::shared_ptr<TimelineItemModel> timeline, int clipId, int trackId, int position);
    static void showClipKeyframes(std::shared_ptr<TimelineItemModel> timeline, int clipId, bool value);
    static void showCompositionKeyframes(std::shared_ptr<TimelineItemModel> timeline, int compoId, bool value);
    static bool changeClipState(std::shared_ptr<TimelineItemModel> timeline, int clipId, PlaylistState::ClipState status);
    static bool changeClipState(std::shared_ptr<TimelineItemModel> timeline, int clipId, PlaylistState::ClipState status, Fun &undo, Fun &redo);
    static bool requestSplitAudio(std::shared_ptr<TimelineItemModel> timeline, int clipId, int audioTarget);
    static void setCompositionATrack(std::shared_ptr<TimelineItemModel> timeline, int cid, int aTrack);
};

#endif
