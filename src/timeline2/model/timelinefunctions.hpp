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

#include <QDir>

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
       @param position: position (in frames from the beginning of the timeline) where to cut
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
    static bool requestMultipleClipsInsertion(std::shared_ptr<TimelineItemModel> timeline, const QStringList &binIds, int trackId, int position,
                                              QList<int> &clipIds, bool logUndo, bool refreshView);

    static int requestSpacerStartOperation(std::shared_ptr<TimelineItemModel> timeline, int trackId, int position);
    static bool requestSpacerEndOperation(std::shared_ptr<TimelineItemModel> timeline, int itemId, int startPosition, int endPosition);
    static bool extractZone(std::shared_ptr<TimelineItemModel> timeline, QVector<int> tracks, QPoint zone, bool liftOnly);
    static bool liftZone(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo);
    static bool removeSpace(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo);
    static bool insertSpace(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo);
    static bool insertZone(std::shared_ptr<TimelineItemModel> timeline, QList<int> trackIds, const QString &binId, int insertFrame, QPoint zone,
                           bool overwrite);

    static bool requestItemCopy(std::shared_ptr<TimelineItemModel> timeline, int clipId, int trackId, int position);
    static void showClipKeyframes(std::shared_ptr<TimelineItemModel> timeline, int clipId, bool value);
    static void showCompositionKeyframes(std::shared_ptr<TimelineItemModel> timeline, int compoId, bool value);

    /* @brief If the clip is activated, disable, otherwise enable
     * @param timeline: pointer to the timeline that we modify
     * @param clipId: Id of the clip to modify
     * @param status: target status of the clip
     This function creates an undo object and returns true on success
     */
    static bool switchEnableState(std::shared_ptr<TimelineItemModel> timeline, int clipId);
    /* @brief change the clip state and accumulates for undo/redo
     */
    static bool changeClipState(std::shared_ptr<TimelineItemModel> timeline, int clipId, PlaylistState::ClipState status, Fun &undo, Fun &redo);

    static bool requestSplitAudio(std::shared_ptr<TimelineItemModel> timeline, int clipId, int audioTarget);
    static bool requestSplitVideo(std::shared_ptr<TimelineItemModel> timeline, int clipId, int videoTarget);
    static void setCompositionATrack(std::shared_ptr<TimelineItemModel> timeline, int cid, int aTrack);
    static void enableMultitrackView(std::shared_ptr<TimelineItemModel> timeline, bool enable);
    static void saveTimelineSelection(std::shared_ptr<TimelineItemModel> timeline, QList<int> selection, QDir targetDir);
    /** @brief returns the number of same type tracks between 2 tracks
     */
    static int getTrackOffset(std::shared_ptr<TimelineItemModel> timeline, int startTrack, int destTrack);
    /** @brief returns an offset track id
     */
    static int getOffsetTrackId(std::shared_ptr<TimelineItemModel> timeline, int startTrack, int offset, bool audioOffset);
};

#endif
