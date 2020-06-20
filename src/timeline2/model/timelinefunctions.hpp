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
    static bool requestClipCut(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int position, Fun &undo, Fun &redo);
    /* This is the same function, except that it accumulates undo/redo and do not deal with groups. Do not call directly */
    static bool processClipCut(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int position, int &newId, Fun &undo, Fun &redo);

    /* Cuts all clips at given position */
    static bool requestClipCutAll(std::shared_ptr<TimelineItemModel> timeline, int position);

    /* @brief Makes a perfect clone of a given clip, but do not insert it */
    static bool cloneClip(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int &newId, PlaylistState::ClipState state, Fun &undo, Fun &redo);

    /* @brief Creates a string representation of the given clips, that can then be pasted using pasteClips(). Return an empty string on failure */
    static QString copyClips(const std::shared_ptr<TimelineItemModel> &timeline, const std::unordered_set<int> &itemIds);
    /* @brief Paste the clips as described by the string. Returns true on success*/
    static bool pasteClips(const std::shared_ptr<TimelineItemModel> &timeline, const QString &pasteString, int trackId, int position);
    static bool pasteClips(const std::shared_ptr<TimelineItemModel> &timeline, const QString &pasteString, int trackId, int position, Fun &undo, Fun &redo);
    static bool pasteTimelineClips(const std::shared_ptr<TimelineItemModel> &timeline, QDomDocument copiedItems, int position);
    static bool pasteTimelineClips(const std::shared_ptr<TimelineItemModel> &timeline, QDomDocument copiedItems, int position, Fun &timeline_undo, Fun &timeline_redo, bool pushToStack);

    /* @brief Request the addition of multiple clips to the timeline
     * If the addition of any of the clips fails, the entire operation is undone.
     * @returns true on success, false otherwise.
     * @param binIds the list of bin ids to be inserted
     * @param trackId the track where the insertion should happen
     * @param position the position at which the clips should be inserted
     * @param clipIds a return parameter with the ids assigned to the clips if success, empty otherwise
     */
    static bool requestMultipleClipsInsertion(const std::shared_ptr<TimelineItemModel> &timeline, const QStringList &binIds, int trackId, int position,
                                              QList<int> &clipIds, bool logUndo, bool refreshView);

    /** @brief This function will find the blank located in the given track at the given position and remove it
        @returns true on success, false otherwise
        @param trackId id of the track to search in
        @param position of the blank
        @param affectAllTracks if true, the same blank will be removed from all tracks. Note that all the tracks must have a blank at least that big in that
       position
    */
    static bool requestDeleteBlankAt(const std::shared_ptr<TimelineItemModel> &timeline, int trackId, int position, bool affectAllTracks);

    static int requestSpacerStartOperation(const std::shared_ptr<TimelineItemModel> &timeline, int trackId, int position);
    static bool requestSpacerEndOperation(const std::shared_ptr<TimelineItemModel> &timeline, int itemId, int startPosition, int endPosition, int affectedTrack);
    static bool extractZone(const std::shared_ptr<TimelineItemModel> &timeline, QVector<int> tracks, QPoint zone, bool liftOnly);
    static bool liftZone(const std::shared_ptr<TimelineItemModel> &timeline, int trackId, QPoint zone, Fun &undo, Fun &redo);
    static bool removeSpace(const std::shared_ptr<TimelineItemModel> &timeline, QPoint zone, Fun &undo, Fun &redo, QVector<int> allowedTracks = QVector<int>(), bool useTargets = true);

    /** @brief This function will insert a blank space starting at zone.x, and ending at zone.y. This will affect all the tracks
        @returns true on success, false otherwise
    */
    static bool requestInsertSpace(const std::shared_ptr<TimelineItemModel> &timeline, QPoint zone, Fun &undo, Fun &redo, QVector<int> allowedTracks = QVector<int> ());
    static bool insertZone(const std::shared_ptr<TimelineItemModel> &timeline, QList<int> trackIds, const QString &binId, int insertFrame, QPoint zone, bool overwrite, bool useTargets = true);
    static bool insertZone(const std::shared_ptr<TimelineItemModel> &timeline, QList<int> trackIds, const QString &binId, int insertFrame, QPoint zone, bool overwrite, bool useTargets, Fun &undo, Fun &redo);

    static bool requestItemCopy(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int trackId, int position);
    static void showClipKeyframes(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, bool value);
    static void showCompositionKeyframes(const std::shared_ptr<TimelineItemModel> &timeline, int compoId, bool value);

    /* @brief If the clip is activated, disable, otherwise enable
     * @param timeline: pointer to the timeline that we modify
     * @param clipId: Id of the clip to modify
     * @param status: target status of the clip
     This function creates an undo object and returns true on success
     */
    static bool switchEnableState(const std::shared_ptr<TimelineItemModel> &timeline, std::unordered_set<int> selection);
    /* @brief change the clip state and accumulates for undo/redo
     */
    static bool changeClipState(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, PlaylistState::ClipState status, Fun &undo, Fun &redo);

    static bool requestSplitAudio(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int audioTarget);
    static bool requestSplitVideo(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int videoTarget);
    static void setCompositionATrack(const std::shared_ptr<TimelineItemModel> &timeline, int cid, int aTrack);
    static QStringList enableMultitrackView(const std::shared_ptr<TimelineItemModel> &timeline, bool enable, bool refresh);
    static void saveTimelineSelection(const std::shared_ptr<TimelineItemModel> &timeline, const std::unordered_set<int> &selection, const QDir &targetDir);
    /** @brief returns the number of same type tracks between 2 tracks
     */
    static int getTrackOffset(const std::shared_ptr<TimelineItemModel> &timeline, int startTrack, int destTrack);
    /** @brief returns an offset track id
     */
    static int getOffsetTrackId(const std::shared_ptr<TimelineItemModel> &timeline, int startTrack, int offset, bool audioOffset);
    /** @brief returns a list of ids for all audio tracks and all video tracks
     */
    static QPair<QList<int>, QList<int>> getAVTracksIds(const std::shared_ptr<TimelineItemModel> &timeline);

    /** @brief This function breaks group is an item in the zone is grouped with an item outside of selected tracks
     */
    static bool breakAffectedGroups(const std::shared_ptr<TimelineItemModel> &timeline, QVector<int> tracks, QPoint zone, Fun &undo, Fun &redo);

    /** @brief This function extracts the content of an xml playlist file and converts it to json paste format
     */
    static QDomDocument extractClip(const std::shared_ptr<TimelineItemModel> &timeline, int cid, const QString &binId);
};

#endif
