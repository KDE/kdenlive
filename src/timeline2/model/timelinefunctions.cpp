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
#include "compositionmodel.hpp"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "groupsmodel.hpp"
#include "timelineitemmodel.hpp"
#include "trackmodel.hpp"
#include "transitions/transitionsrepository.hpp"

#include <QApplication>
#include <QDebug>
#include <QInputDialog>
#include <klocalizedstring.h>

bool TimelineFunctions::copyClip(std::shared_ptr<TimelineItemModel> timeline, int clipId, int &newId, PlaylistState::ClipState state, Fun &undo, Fun &redo)
{
    // Special case: slowmotion clips
    double clipSpeed = timeline->m_allClips[clipId]->getSpeed();
    bool res = timeline->requestClipCreation(timeline->getClipBinId(clipId), newId, state, clipSpeed, undo, redo);
    timeline->m_allClips[newId]->m_endlessResize = timeline->m_allClips[clipId]->m_endlessResize;

    // copy useful timeline properties
    timeline->m_allClips[clipId]->passTimelineProperties(timeline->m_allClips[newId]);

    int duration = timeline->getClipPlaytime(clipId);
    int init_duration = timeline->getClipPlaytime(newId);
    if (duration != init_duration) {
        int in = timeline->m_allClips[clipId]->getIn();
        res = res && timeline->requestItemResize(newId, init_duration - in, false, true, undo, redo);
        res = res && timeline->requestItemResize(newId, duration, true, true, undo, redo);
    }
    if (!res) {
        return false;
    }
    std::shared_ptr<EffectStackModel> sourceStack = timeline->getClipEffectStackModel(clipId);
    std::shared_ptr<EffectStackModel> destStack = timeline->getClipEffectStackModel(newId);
    destStack->importEffects(sourceStack, state);
    return res;
}

bool TimelineFunctions::requestMultipleClipsInsertion(std::shared_ptr<TimelineItemModel> timeline, const QStringList &binIds, int trackId, int position,
                                                      QList<int> &clipIds, bool logUndo, bool refreshView)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };

    for (const QString &binId : binIds) {
        int clipId;
        if (timeline->requestClipInsertion(binId, trackId, position, clipId, logUndo, refreshView, true, undo, redo)) {
            clipIds.append(clipId);
            position += timeline->getItemPlaytime(clipId);
        } else {
            undo();
            clipIds.clear();
            return false;
        }
    }

    if (logUndo) {
        pCore->pushUndo(undo, redo, i18n("Insert Clips"));
    }

    return true;
}

bool TimelineFunctions::processClipCut(std::shared_ptr<TimelineItemModel> timeline, int clipId, int position, int &newId, Fun &undo, Fun &redo)
{
    int trackId = timeline->getClipTrackId(clipId);
    int trackDuration = timeline->getTrackById_const(trackId)->trackDuration();
    int start = timeline->getClipPosition(clipId);
    int duration = timeline->getClipPlaytime(clipId);
    if (start > position || (start + duration) < position) {
        return false;
    }
    PlaylistState::ClipState state = timeline->m_allClips[clipId]->clipState();
    bool res = copyClip(timeline, clipId, newId, state, undo, redo);
    res = res && timeline->requestItemResize(clipId, position - start, true, true, undo, redo);
    int newDuration = timeline->getClipPlaytime(clipId);
    // parse effects
    std::shared_ptr<EffectStackModel> sourceStack = timeline->getClipEffectStackModel(clipId);
    sourceStack->cleanFadeEffects(true, undo, redo);
    std::shared_ptr<EffectStackModel> destStack = timeline->getClipEffectStackModel(newId);
    destStack->cleanFadeEffects(false, undo, redo);
    res = res && timeline->requestItemResize(newId, duration - newDuration, false, true, undo, redo);
    // The next requestclipmove does not check for duration change since we don't invalidate timeline, so check duration change now
    bool durationChanged = trackDuration != timeline->getTrackById_const(trackId)->trackDuration();
    res = res && timeline->requestClipMove(newId, trackId, position, true, false, undo, redo);
    if (durationChanged) {
        // Track length changed, check project duration
        Fun updateDuration = [timeline]() {
            timeline->updateDuration();
            return true;
        };
        updateDuration();
        PUSH_LAMBDA(updateDuration, redo);
    }
    return res;
}

bool TimelineFunctions::requestClipCut(std::shared_ptr<TimelineItemModel> timeline, int clipId, int position)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool result = TimelineFunctions::requestClipCut(timeline, clipId, position, undo, redo);
    if (result) {
        pCore->pushUndo(undo, redo, i18n("Cut clip"));
    }
    return result;
}

bool TimelineFunctions::requestClipCut(std::shared_ptr<TimelineItemModel> timeline, int clipId, int position, Fun &undo, Fun &redo)
{
    const std::unordered_set<int> clips = timeline->getGroupElements(clipId);
    int root = timeline->m_groups->getRootId(clipId);
    std::unordered_set<int> topElements;
    if (timeline->m_temporarySelectionGroup == root) {
        topElements = timeline->m_groups->getDirectChildren(root);
    } else {
        topElements.insert(root);
    }
    // We need to call clearSelection before attempting the split or the group split will be corrupted by the selection group (no undo support)
    bool isSelected = pCore->isSelected(clipId);
    pCore->clearSelection();
    int count = 0;
    QList<int> newIds;
    for (int cid : clips) {
        int start = timeline->getClipPosition(cid);
        int duration = timeline->getClipPlaytime(cid);
        if (start < position && (start + duration) > position) {
            count++;
            int newId;
            bool res = processClipCut(timeline, cid, position, newId, undo, redo);
            if (!res) {
                bool undone = undo();
                Q_ASSERT(undone);
                return false;
            }
            // splitted elements go temporarily in the same group as original ones.
            timeline->m_groups->setInGroupOf(newId, cid, undo, redo);
            newIds << newId;
        }
    }
    if (count > 0 && timeline->m_groups->isInGroup(clipId)) {
        // we now split the group hierarchy.
        // As a splitting criterion, we compare start point with split position
        auto criterion = [timeline, position](int cid) { return timeline->getClipPosition(cid) < position; };
        bool res = true;
        for (const int topId : topElements) {
            res = res & timeline->m_groups->split(topId, criterion, undo, redo);
        }
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    if (isSelected && !newIds.isEmpty()) {
        pCore->selectItem(newIds.first());
    }
    return count > 0;
}

int TimelineFunctions::requestSpacerStartOperation(std::shared_ptr<TimelineItemModel> timeline, int trackId, int position)
{
    std::unordered_set<int> clips = timeline->getItemsInRange(trackId, position, -1);
    if (clips.size() > 0) {
        timeline->requestClipsGroup(clips, false, GroupType::Selection);
        return (*clips.cbegin());
    }
    return -1;
}

bool TimelineFunctions::requestSpacerEndOperation(std::shared_ptr<TimelineItemModel> timeline, int itemId, int startPosition, int endPosition)
{
    // Move group back to original position
    int track = timeline->getItemTrackId(itemId);
    bool isClip = timeline->isClip(itemId);
    if (isClip) {
        timeline->requestClipMove(itemId, track, startPosition, false, false);
    } else {
        timeline->requestCompositionMove(itemId, track, startPosition, false, false);
    }
    std::unordered_set<int> clips = timeline->getGroupElements(itemId);
    // break group
    pCore->clearSelection();
    // Start undoable command
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int res = timeline->requestClipsGroup(clips, undo, redo);
    bool final = false;
    if (res > -1) {
        if (clips.size() > 1) {
            final = timeline->requestGroupMove(itemId, res, 0, endPosition - startPosition, true, true, undo, redo);
        } else {
            // only 1 clip to be moved
            if (isClip) {
                final = timeline->requestClipMove(itemId, track, endPosition, true, true, undo, redo);
            } else {
                final = timeline->requestCompositionMove(itemId, track, -1, endPosition, true, true, undo, redo);
            }
        }
    }
    if (final && clips.size() > 1) {
        final = timeline->requestClipUngroup(itemId, undo, redo);
    }
    if (final) {
        pCore->pushUndo(undo, redo, i18n("Insert space"));
        return true;
    }
    return false;
}

bool TimelineFunctions::extractZone(std::shared_ptr<TimelineItemModel> timeline, QVector<int> tracks, QPoint zone, bool liftOnly)
{
    // Start undoable command
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool result = true;
    for (int trackId : tracks) {
        result = result && TimelineFunctions::liftZone(timeline, trackId, zone, undo, redo);
    }
    if (result && !liftOnly) {
        result = TimelineFunctions::removeSpace(timeline, -1, zone, undo, redo);
    }
    pCore->pushUndo(undo, redo, liftOnly ? i18n("Lift zone") : i18n("Extract zone"));
    return result;
}

bool TimelineFunctions::insertZone(std::shared_ptr<TimelineItemModel> timeline, QList<int> trackIds, const QString &binId, int insertFrame, QPoint zone,
                                   bool overwrite)
{
    // Start undoable command
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool result = false;
    int trackId = trackIds.takeFirst();
    if (overwrite) {
        result = TimelineFunctions::liftZone(timeline, trackId, QPoint(insertFrame, insertFrame + (zone.y() - zone.x())), undo, redo);
        if (!trackIds.isEmpty()) {
            result =
                result && TimelineFunctions::liftZone(timeline, trackIds.takeFirst(), QPoint(insertFrame, insertFrame + (zone.y() - zone.x())), undo, redo);
        }
    } else {
        // Cut all tracks
        auto it = timeline->m_allTracks.cbegin();
        while (it != timeline->m_allTracks.cend()) {
            int target_track = (*it)->getId();
            if (timeline->getTrackById_const(target_track)->isLocked()) {
                ++it;
                continue;
            }
            int startClipId = timeline->getClipByPosition(target_track, insertFrame);
            if (startClipId > -1) {
                // There is a clip, cut it
                TimelineFunctions::requestClipCut(timeline, startClipId, insertFrame, undo, redo);
            }
            ++it;
        }
        result = TimelineFunctions::insertSpace(timeline, trackId, QPoint(insertFrame, insertFrame + (zone.y() - zone.x())), undo, redo);
    }
    if (result) {
        int newId = -1;
        QString binClipId = QString("%1/%2/%3").arg(binId).arg(zone.x()).arg(zone.y() - 1);
        result = timeline->requestClipInsertion(binClipId, trackId, insertFrame, newId, true, true, true, undo, redo);
        if (result) {
            pCore->pushUndo(undo, redo, overwrite ? i18n("Overwrite zone") : i18n("Insert zone"));
        }
    }
    if (!result) {
        undo();
    }
    return result;
}

bool TimelineFunctions::liftZone(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo)
{
    // Check if there is a clip at start point
    int startClipId = timeline->getClipByPosition(trackId, zone.x());
    if (startClipId > -1) {
        // There is a clip, cut it
        if (timeline->getClipPosition(startClipId) < zone.x()) {
            qDebug() << "/// CUTTING AT START: " << zone.x() << ", ID: " << startClipId;
            TimelineFunctions::requestClipCut(timeline, startClipId, zone.x(), undo, redo);
            qDebug() << "/// CUTTING AT START DONE";
        }
    }
    int endClipId = timeline->getClipByPosition(trackId, zone.y());
    if (endClipId > -1) {
        // There is a clip, cut it
        if (timeline->getClipPosition(endClipId) + timeline->getClipPlaytime(endClipId) > zone.y()) {
            qDebug() << "/// CUTTING AT END: " << zone.y() << ", ID: " << endClipId;
            TimelineFunctions::requestClipCut(timeline, endClipId, zone.y(), undo, redo);
            qDebug() << "/// CUTTING AT END DONE";
        }
    }
    std::unordered_set<int> clips = timeline->getItemsInRange(trackId, zone.x(), zone.y());
    for (const auto &clipId : clips) {
        timeline->requestItemDeletion(clipId, undo, redo);
    }
    return true;
}

bool TimelineFunctions::removeSpace(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo)
{
    Q_UNUSED(trackId)

    std::unordered_set<int> clips = timeline->getItemsInRange(-1, zone.y() - 1, -1, true);
    bool result = false;
    if (clips.size() > 0) {
        int clipId = *clips.begin();
        if (clips.size() > 1) {
            int res = timeline->requestClipsGroup(clips, undo, redo);
            if (res > -1) {
                result = timeline->requestGroupMove(clipId, res, 0, zone.x() - zone.y(), true, true, undo, redo);
                if (result) {
                    result = timeline->requestClipUngroup(clipId, undo, redo);
                }
                if (!result) {
                    undo();
                }
            }
        } else {
            // only 1 clip to be moved
            int clipStart = timeline->getItemPosition(clipId);
            result = timeline->requestClipMove(clipId, timeline->getItemTrackId(clipId), clipStart - (zone.y() - zone.x()), true, true, undo, redo);
        }
    }
    return result;
}

bool TimelineFunctions::insertSpace(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo)
{
    Q_UNUSED(trackId)

    std::unordered_set<int> clips = timeline->getItemsInRange(-1, zone.x(), -1, true);
    bool result = true;
    if (clips.size() > 0) {
        int clipId = *clips.begin();
        if (clips.size() > 1) {
            int res = timeline->requestClipsGroup(clips, undo, redo);
            if (res > -1) {
                result = timeline->requestGroupMove(clipId, res, 0, zone.y() - zone.x(), true, true, undo, redo);
                if (result) {
                    result = timeline->requestClipUngroup(clipId, undo, redo);
                } else {
                    pCore->displayMessage(i18n("Cannot move selected group"), ErrorMessage);
                }
            }
        } else {
            // only 1 clip to be moved
            int clipStart = timeline->getItemPosition(clipId);
            result = timeline->requestClipMove(clipId, timeline->getItemTrackId(clipId), clipStart + (zone.y() - zone.x()), true, true, undo, redo);
        }
    }
    return result;
}

bool TimelineFunctions::requestItemCopy(std::shared_ptr<TimelineItemModel> timeline, int clipId, int trackId, int position)
{
    Q_ASSERT(timeline->isClip(clipId) || timeline->isComposition(clipId));
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    int deltaTrack = timeline->getTrackPosition(trackId) - timeline->getTrackPosition(timeline->getItemTrackId(clipId));
    int deltaPos = position - timeline->getItemPosition(clipId);
    std::unordered_set<int> allIds = timeline->getGroupElements(clipId);
    std::unordered_map<int, int> mapping; // keys are ids of the source clips, values are ids of the copied clips
    bool res = true;
    for (int id : allIds) {
        int newId = -1;
        if (timeline->isClip(id)) {
            PlaylistState::ClipState state = timeline->m_allClips[id]->clipState();
            res = copyClip(timeline, id, newId, state, undo, redo);
            res = res && (newId != -1);
        }
        int target_position = timeline->getItemPosition(id) + deltaPos;
        int target_track_position = timeline->getTrackPosition(timeline->getItemTrackId(id)) + deltaTrack;
        if (target_track_position >= 0 && target_track_position < timeline->getTracksCount()) {
            auto it = timeline->m_allTracks.cbegin();
            std::advance(it, target_track_position);
            int target_track = (*it)->getId();
            if (timeline->isClip(id)) {
                res = res && timeline->requestClipMove(newId, target_track, target_position, true, true, undo, redo);
            } else {
                const QString &transitionId = timeline->m_allCompositions[id]->getAssetId();
                std::unique_ptr<Mlt::Properties> transProps(timeline->m_allCompositions[id]->properties());
                res = res & timeline->requestCompositionInsertion(transitionId, target_track, -1, target_position,
                                                                  timeline->m_allCompositions[id]->getPlaytime(), std::move(transProps), newId, undo, redo);
            }
        } else {
            res = false;
        }
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
        mapping[id] = newId;
    }
    qDebug() << "Successful copy, coping groups...";
    res = timeline->m_groups->copyGroups(mapping, undo, redo);
    if (!res) {
        bool undone = undo();
        Q_ASSERT(undone);
        return false;
    }
    return true;
}

void TimelineFunctions::showClipKeyframes(std::shared_ptr<TimelineItemModel> timeline, int clipId, bool value)
{
    timeline->m_allClips[clipId]->setShowKeyframes(value);
    QModelIndex modelIndex = timeline->makeClipIndexFromID(clipId);
    timeline->dataChanged(modelIndex, modelIndex, {TimelineModel::ShowKeyframesRole});
}

void TimelineFunctions::showCompositionKeyframes(std::shared_ptr<TimelineItemModel> timeline, int compoId, bool value)
{
    timeline->m_allCompositions[compoId]->setShowKeyframes(value);
    QModelIndex modelIndex = timeline->makeCompositionIndexFromID(compoId);
    timeline->dataChanged(modelIndex, modelIndex, {TimelineModel::ShowKeyframesRole});
}

bool TimelineFunctions::switchEnableState(std::shared_ptr<TimelineItemModel> timeline, int clipId)
{
    PlaylistState::ClipState oldState = timeline->getClipPtr(clipId)->clipState();
    PlaylistState::ClipState state = PlaylistState::Disabled;
    bool disable = true;
    if (oldState == PlaylistState::Disabled) {
        state = timeline->getTrackById_const(timeline->getClipTrackId(clipId))->trackType();
        disable = false;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = changeClipState(timeline, clipId, state, undo, redo);
    if (result) {
        pCore->pushUndo(undo, redo, disable ? i18n("Disable clip") : i18n("Enable clip"));
    }
    return result;
}

bool TimelineFunctions::changeClipState(std::shared_ptr<TimelineItemModel> timeline, int clipId, PlaylistState::ClipState status, Fun &undo, Fun &redo)
{
    int track = timeline->getClipTrackId(clipId);
    int start = -1;
    int end = -1;
    if (track > -1) {
        if (!timeline->getTrackById_const(track)->isAudioTrack()) {
            start = timeline->getItemPosition(clipId);
            end = start + timeline->getItemPlaytime(clipId);
        }
    }
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };

    bool result = timeline->m_allClips[clipId]->setClipState(status, local_undo, local_redo);
    Fun local_update = [start, end, timeline]() {
        if (start > -1) {
            timeline->invalidateZone(start, end);
            timeline->checkRefresh(start, end);
        }
        return true;
    };
    if (start > -1) {
        local_update();
        PUSH_LAMBDA(local_update, local_redo);
        PUSH_LAMBDA(local_update, local_undo);
    }
    UPDATE_UNDO_REDO_NOLOCK(local_redo, local_undo, undo, redo);
    return result;
}

bool TimelineFunctions::requestSplitAudio(std::shared_ptr<TimelineItemModel> timeline, int clipId, int audioTarget)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    const std::unordered_set<int> clips = timeline->getGroupElements(clipId);
    bool done = false;
    // Now clear selection so we don't mess with groups
    pCore->clearSelection();
    for (int cid : clips) {
        if (!timeline->getClipPtr(cid)->canBeAudio() || timeline->getClipPtr(cid)->clipState() == PlaylistState::AudioOnly) {
            // clip without audio or audio only, skip
            pCore->displayMessage(i18n("One or more clips don't have audio, or are already audio"), ErrorMessage);
            return false;
        }
        int position = timeline->getClipPosition(cid);
        int track = timeline->getClipTrackId(cid);
        QList<int> possibleTracks = audioTarget >= 0 ? QList<int>() << audioTarget : timeline->getLowerTracksId(track, TrackType::AudioTrack);
        if (possibleTracks.isEmpty()) {
            // No available audio track for splitting, abort
            undo();
            pCore->displayMessage(i18n("No available audio track for split operation"), ErrorMessage);
            return false;
        }
        int newId;
        bool res = copyClip(timeline, cid, newId, PlaylistState::AudioOnly, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            pCore->displayMessage(i18n("Audio split failed"), ErrorMessage);
            return false;
        }
        bool success = false;
        while (!success && !possibleTracks.isEmpty()) {
            int newTrack = possibleTracks.takeFirst();
            success = timeline->requestClipMove(newId, newTrack, position, true, false, undo, redo);
        }
        TimelineFunctions::changeClipState(timeline, cid, PlaylistState::VideoOnly, undo, redo);
        success = success && timeline->m_groups->createGroupAtSameLevel(cid, std::unordered_set<int>{newId}, GroupType::AVSplit, undo, redo);
        if (!success) {
            bool undone = undo();
            Q_ASSERT(undone);
            pCore->displayMessage(i18n("Audio split failed"), ErrorMessage);
            return false;
        }
        done = true;
    }
    if (done) {
        pCore->pushUndo(undo, redo, i18n("Split Audio"));
    }
    return done;
}

bool TimelineFunctions::requestSplitVideo(std::shared_ptr<TimelineItemModel> timeline, int clipId, int videoTarget)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    const std::unordered_set<int> clips = timeline->getGroupElements(clipId);
    bool done = false;
    // Now clear selection so we don't mess with groups
    pCore->clearSelection();
    for (int cid : clips) {
        if (!timeline->getClipPtr(cid)->canBeVideo() || timeline->getClipPtr(cid)->clipState() == PlaylistState::VideoOnly) {
            // clip without audio or audio only, skip
            continue;
        }
        int position = timeline->getClipPosition(cid);
        QList<int> possibleTracks = QList<int>() << videoTarget;
        if (possibleTracks.isEmpty()) {
            // No available audio track for splitting, abort
            undo();
            pCore->displayMessage(i18n("No available video track for split operation"), ErrorMessage);
            return false;
        }
        int newId;
        bool res = copyClip(timeline, cid, newId, PlaylistState::VideoOnly, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            pCore->displayMessage(i18n("Video split failed"), ErrorMessage);
            return false;
        }
        bool success = false;
        while (!success && !possibleTracks.isEmpty()) {
            int newTrack = possibleTracks.takeFirst();
            success = timeline->requestClipMove(newId, newTrack, position, true, false, undo, redo);
        }
        TimelineFunctions::changeClipState(timeline, cid, PlaylistState::AudioOnly, undo, redo);
        success = success && timeline->m_groups->createGroupAtSameLevel(cid, std::unordered_set<int>{newId}, GroupType::AVSplit, undo, redo);
        if (!success) {
            bool undone = undo();
            Q_ASSERT(undone);
            pCore->displayMessage(i18n("Video split failed"), ErrorMessage);
            return false;
        }
        done = true;
    }
    if (done) {
        pCore->pushUndo(undo, redo, i18n("Split Video"));
    }
    return done;
}

void TimelineFunctions::setCompositionATrack(std::shared_ptr<TimelineItemModel> timeline, int cid, int aTrack)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    std::shared_ptr<CompositionModel> compo = timeline->getCompositionPtr(cid);
    int previousATrack = compo->getATrack();
    int previousAutoTrack = compo->getForcedTrack() == -1;
    bool autoTrack = aTrack < 0;
    if (autoTrack) {
        // Automatic track compositing, find lower video track
        aTrack = timeline->getPreviousVideoTrackPos(compo->getCurrentTrackId());
    }
    int start = timeline->getItemPosition(cid);
    int end = start + timeline->getItemPlaytime(cid);
    Fun local_redo = [timeline, cid, aTrack, autoTrack, start, end]() {
        QScopedPointer<Mlt::Field> field(timeline->m_tractor->field());
        field->lock();
        timeline->getCompositionPtr(cid)->setForceTrack(!autoTrack);
        timeline->getCompositionPtr(cid)->setATrack(aTrack, aTrack <= 0 ? -1 : timeline->getTrackIndexFromPosition(aTrack - 1));
        field->unlock();
        QModelIndex modelIndex = timeline->makeCompositionIndexFromID(cid);
        timeline->dataChanged(modelIndex, modelIndex, {TimelineModel::ItemATrack});
        timeline->invalidateZone(start, end);
        timeline->checkRefresh(start, end);
        return true;
    };
    Fun local_undo = [timeline, cid, previousATrack, previousAutoTrack, start, end]() {
        QScopedPointer<Mlt::Field> field(timeline->m_tractor->field());
        field->lock();
        timeline->getCompositionPtr(cid)->setForceTrack(!previousAutoTrack);
        timeline->getCompositionPtr(cid)->setATrack(previousATrack, previousATrack <= 0 ? -1 : timeline->getTrackIndexFromPosition(previousATrack - 1));
        field->unlock();
        QModelIndex modelIndex = timeline->makeCompositionIndexFromID(cid);
        timeline->dataChanged(modelIndex, modelIndex, {TimelineModel::ItemATrack});
        timeline->invalidateZone(start, end);
        timeline->checkRefresh(start, end);
        return true;
    };
    if (local_redo()) {
        PUSH_LAMBDA(local_undo, undo);
        PUSH_LAMBDA(local_redo, redo);
    }
    pCore->pushUndo(undo, redo, i18n("Change Composition Track"));
}

void TimelineFunctions::enableMultitrackView(std::shared_ptr<TimelineItemModel> timeline, bool enable)
{
    QList<int> videoTracks;
    for (const auto &track : timeline->m_iteratorTable) {
        if (timeline->getTrackById_const(track.first)->isAudioTrack() || timeline->getTrackById_const(track.first)->isHidden()) {
            continue;
        }
        videoTracks << track.first;
    }
    if (videoTracks.size() < 2) {
        pCore->displayMessage(i18n("Cannot enable multitrack view on a single track"), InformationMessage);
    }
    // First, dis/enable track compositing
    QScopedPointer<Mlt::Service> service(timeline->m_tractor->field());
    Mlt::Field *field = timeline->m_tractor->field();
    field->lock();
    while ((service != nullptr) && service->is_valid()) {
        if (service->type() == transition_type) {
            Mlt::Transition t((mlt_transition)service->get_service());
            QString serviceName = t.get("mlt_service");
            int added = t.get_int("internal_added");
            if (added == 237 && serviceName != QLatin1String("mix")) {
                // remove all compositing transitions
                t.set("disable", enable ? "1" : nullptr);
            } else if (!enable && added == 200) {
                field->disconnect_service(t);
            }
        }
        service.reset(service->producer());
    }
    if (enable) {
        for (int i = 0; i < videoTracks.size(); ++i) {
            Mlt::Transition transition(*timeline->m_tractor->profile(), "composite");
            transition.set("mlt_service", "composite");
            transition.set("a_track", 0);
            transition.set("b_track", timeline->getTrackMltIndex(videoTracks.at(i)));
            transition.set("distort", 0);
            transition.set("aligned", 0);
            // 200 is an arbitrary number so we can easily remove these transition later
            transition.set("internal_added", 200);
            QString geometry;
            switch (i) {
            case 0:
                switch (videoTracks.size()) {
                case 2:
                    geometry = QStringLiteral("0 0 50% 100%");
                    break;
                case 3:
                    geometry = QStringLiteral("0 0 33% 100%");
                    break;
                case 4:
                    geometry = QStringLiteral("0 0 50% 50%");
                    break;
                case 5:
                case 6:
                    geometry = QStringLiteral("0 0 33% 50%");
                    break;
                default:
                    geometry = QStringLiteral("0 0 33% 33%");
                    break;
                }
                break;
            case 1:
                switch (videoTracks.size()) {
                case 2:
                    geometry = QStringLiteral("50% 0 50% 100%");
                    break;
                case 3:
                    geometry = QStringLiteral("33% 0 33% 100%");
                    break;
                case 4:
                    geometry = QStringLiteral("50% 0 50% 50%");
                    break;
                case 5:
                case 6:
                    geometry = QStringLiteral("33% 0 33% 50%");
                    break;
                default:
                    geometry = QStringLiteral("33% 0 33% 33%");
                    break;
                }
                break;
            case 2:
                switch (videoTracks.size()) {
                case 3:
                    geometry = QStringLiteral("66% 0 33% 100%");
                    break;
                case 4:
                    geometry = QStringLiteral("0 50% 50% 50%");
                    break;
                case 5:
                case 6:
                    geometry = QStringLiteral("66% 0 33% 50%");
                    break;
                default:
                    geometry = QStringLiteral("66% 0 33% 33%");
                    break;
                }
                break;
            case 3:
                switch (videoTracks.size()) {
                case 4:
                    geometry = QStringLiteral("50% 50% 50% 50%");
                    break;
                case 5:
                case 6:
                    geometry = QStringLiteral("0 50% 33% 50%");
                    break;
                default:
                    geometry = QStringLiteral("0 33% 33% 33%");
                    break;
                }
                break;
            case 4:
                switch (videoTracks.size()) {
                case 5:
                case 6:
                    geometry = QStringLiteral("33% 50% 33% 50%");
                    break;
                default:
                    geometry = QStringLiteral("33% 33% 33% 33%");
                    break;
                }
                break;
            case 5:
                switch (videoTracks.size()) {
                case 6:
                    geometry = QStringLiteral("66% 50% 33% 50%");
                    break;
                default:
                    geometry = QStringLiteral("66% 33% 33% 33%");
                    break;
                }
                break;
            case 6:
                geometry = QStringLiteral("0 66% 33% 33%");
                break;
            case 7:
                geometry = QStringLiteral("33% 66% 33% 33%");
                break;
            default:
                geometry = QStringLiteral("66% 66% 33% 33%");
                break;
            }
            // Add transition to track:
            transition.set("geometry", geometry.toUtf8().constData());
            transition.set("always_active", 1);
            field->plant_transition(transition, 0, timeline->getTrackMltIndex(videoTracks.at(i)));
        }
    }
    field->unlock();
    timeline->requestMonitorRefresh();
}

void TimelineFunctions::saveTimelineSelection(std::shared_ptr<TimelineItemModel> timeline, QList<int> selection, QDir targetDir)
{
    bool ok;
    QString name = QInputDialog::getText(qApp->activeWindow(), i18n("Add Clip to Library"), i18n("Enter a name for the clip in Library"), QLineEdit::Normal,
                                         QString(), &ok);
    if (name.isEmpty() || !ok) {
        return;
    }
    if (targetDir.exists(name + QStringLiteral(".mlt"))) {
        // TODO: warn and ask for overwrite / rename
    }
    int offset = -1;
    int lowerAudioTrack = -1;
    int lowerVideoTrack = -1;
    QString fullPath = targetDir.absoluteFilePath(name + QStringLiteral(".mlt"));
    // Build a copy of selected tracks.
    QMap<int, int> sourceTracks;
    for (int i : selection) {
        int sourceTrack = timeline->getItemTrackId(i);
        int clipPos = timeline->getItemPosition(i);
        if (offset < 0 || clipPos < offset) {
            offset = clipPos;
        }
        int trackPos = timeline->getTrackMltIndex(sourceTrack);
        if (!sourceTracks.contains(trackPos)) {
            sourceTracks.insert(trackPos, sourceTrack);
        }
    }
    // Build target timeline
    Mlt::Tractor newTractor(*timeline->m_tractor->profile());
    QScopedPointer<Mlt::Field> field(newTractor.field());
    int ix = 0;
    QString composite = TransitionsRepository::get()->getCompositingTransition();
    QMapIterator<int, int> i(sourceTracks);
    QList<Mlt::Transition *> compositions;
    while (i.hasNext()) {
        i.next();
        QScopedPointer<Mlt::Playlist> newTrackPlaylist(new Mlt::Playlist(*newTractor.profile()));
        newTractor.set_track(*newTrackPlaylist, ix);
        // QScopedPointer<Mlt::Producer> trackProducer(newTractor.track(ix));
        int trackId = i.value();
        sourceTracks.insert(timeline->getTrackMltIndex(trackId), ix);
        std::shared_ptr<TrackModel> track = timeline->getTrackById_const(trackId);
        bool isAudio = track->isAudioTrack();
        if (isAudio) {
            newTrackPlaylist->set("hide", 1);
            if (lowerAudioTrack < 0) {
                lowerAudioTrack = ix;
            }
        } else {
            newTrackPlaylist->set("hide", 2);
            if (lowerVideoTrack < 0) {
                lowerVideoTrack = ix;
            }
        }
        for (int itemId : selection) {
            if (timeline->getItemTrackId(itemId) == trackId) {
                // Copy clip on the destination track
                if (timeline->isClip(itemId)) {
                    int clip_position = timeline->m_allClips[itemId]->getPosition();
                    auto clip_loc = track->getClipIndexAt(clip_position);
                    int target_clip = clip_loc.second;
                    QSharedPointer<Mlt::Producer> clip = track->getClipProducer(target_clip);
                    newTrackPlaylist->insert_at(clip_position - offset, clip.data(), 1);
                } else if (timeline->isComposition(itemId)) {
                    // Composition
                    Mlt::Transition *t = new Mlt::Transition(*timeline->m_allCompositions[itemId].get());
                    QString id(t->get("kdenlive_id"));
                    QString internal(t->get("internal_added"));
                    if (internal.isEmpty()) {
                        compositions << t;
                        if (id.isEmpty()) {
                            qDebug() << "// Warning, this should not happen, transition without id: " << t->get("id") << " = " << t->get("mlt_service");
                            t->set("kdenlive_id", t->get("mlt_service"));
                        }
                    }
                }
            }
        }
        ix++;
    }
    // Sort compositions and insert
    if (!compositions.isEmpty()) {
        std::sort(compositions.begin(), compositions.end(), [](Mlt::Transition *a, Mlt::Transition *b) { return a->get_b_track() < b->get_b_track(); });
        while (!compositions.isEmpty()) {
            QScopedPointer<Mlt::Transition> t(compositions.takeFirst());
            if (sourceTracks.contains(t->get_a_track()) && sourceTracks.contains(t->get_b_track())) {
                Mlt::Transition newComposition(*newTractor.profile(), t->get("mlt_service"));
                Mlt::Properties sourceProps(t->get_properties());
                newComposition.inherit(sourceProps);
                QString id(t->get("kdenlive_id"));
                int in = qMax(0, t->get_in() - offset);
                int out = t->get_out() - offset;
                newComposition.set_in_and_out(in, out);
                int a_track = sourceTracks.value(t->get_a_track());
                int b_track = sourceTracks.value(t->get_b_track());
                field->plant_transition(newComposition, a_track, b_track);
            }
        }
    }
    // Track compositing
    i.toFront();
    ix = 0;
    while (i.hasNext()) {
        i.next();
        int trackId = i.value();
        std::shared_ptr<TrackModel> track = timeline->getTrackById_const(trackId);
        bool isAudio = track->isAudioTrack();
        if ((isAudio && ix > lowerAudioTrack) || (!isAudio && ix > lowerVideoTrack)) {
            // add track compositing / mix
            Mlt::Transition t(*newTractor.profile(), isAudio ? "mix" : composite.toUtf8().constData());
            if (isAudio) {
                t.set("sum", 1);
            }
            t.set("always_active", 1);
            t.set("internal_added", 237);
            field->plant_transition(t, isAudio ? lowerAudioTrack : lowerVideoTrack, ix);
        }
        ix++;
    }
    Mlt::Consumer xmlConsumer(*newTractor.profile(), ("xml:" + fullPath).toUtf8().constData());
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.connect(newTractor);
    xmlConsumer.run();
}

int TimelineFunctions::getTrackOffset(std::shared_ptr<TimelineItemModel> timeline, int startTrack, int destTrack)
{
    qDebug() << "+++++++\nGET TRACK OFFSET: " << startTrack << " - " << destTrack;
    int masterTrackMltIndex = timeline->getTrackMltIndex(startTrack);
    int destTrackMltIndex = timeline->getTrackMltIndex(destTrack);
    int offset = 0;
    qDebug() << "+++++++\nGET TRACK MLT: " << masterTrackMltIndex << " - " << destTrackMltIndex;
    if (masterTrackMltIndex == destTrackMltIndex) {
        return offset;
    }
    int step = masterTrackMltIndex > destTrackMltIndex ? -1 : 1;
    bool isAudio = timeline->isAudioTrack(startTrack);
    int track = masterTrackMltIndex;
    while (track != destTrackMltIndex) {
        track += step;
        qDebug() << "+ + +TESTING TRACK: " << track;
        int trackId = timeline->getTrackIndexFromPosition(track - 1);
        if (isAudio == timeline->isAudioTrack(trackId)) {
            offset += step;
        }
    }
    return offset;
}

int TimelineFunctions::getOffsetTrackId(std::shared_ptr<TimelineItemModel> timeline, int startTrack, int offset, bool audioOffset)
{
    int masterTrackMltIndex = timeline->getTrackMltIndex(startTrack);
    bool isAudio = timeline->isAudioTrack(startTrack);
    if (isAudio != audioOffset) {
        offset = -offset;
    }
    qDebug() << "* ** * MASTER INDEX: " << masterTrackMltIndex << ", OFFSET: " << offset;
    while (offset != 0) {
        masterTrackMltIndex += offset > 0 ? 1 : -1;
        qDebug() << "#### TESTING TRACK: " << masterTrackMltIndex;
        if (masterTrackMltIndex < 0) {
            masterTrackMltIndex = 0;
            break;
        } else if (masterTrackMltIndex > (int)timeline->m_allTracks.size()) {
            masterTrackMltIndex = (int)timeline->m_allTracks.size();
            break;
        }
        int trackId = timeline->getTrackIndexFromPosition(masterTrackMltIndex - 1);
        if (timeline->isAudioTrack(trackId) == isAudio) {
            offset += offset > 0 ? -1 : 1;
        }
    }
    return timeline->getTrackIndexFromPosition(masterTrackMltIndex - 1);
}
