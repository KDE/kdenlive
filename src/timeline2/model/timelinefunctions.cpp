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

#include <QDebug>
#include <klocalizedstring.h>

bool TimelineFunctions::copyClip(std::shared_ptr<TimelineItemModel> timeline, int clipId, int &newId, PlaylistState::ClipState state, Fun &undo, Fun &redo)
{
    // Special case: slowmotion clips
    double clipSpeed = timeline->m_allClips[clipId]->getSpeed();
    bool res = timeline->requestClipCreation(timeline->getClipBinId(clipId), newId, state, undo, redo);
    timeline->m_allClips[newId]->m_endlessResize = timeline->m_allClips[clipId]->m_endlessResize;
    // Apply speed effect if necessary
    if (!qFuzzyCompare(clipSpeed, 1.0)) {
        timeline->m_allClips[newId]->useTimewarpProducer(clipSpeed, -1, undo, redo);
    }
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
    destStack->importEffects(sourceStack);
    return res;
}

bool TimelineFunctions::requestMultipleClipsInsertion(std::shared_ptr<TimelineItemModel> timeline, const QStringList &binIds, int trackId, int position, QList<int> &clipIds, bool logUndo, bool refreshView)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };

    for (const QString &binId : binIds) {
        int clipId;
        if (timeline->requestClipInsertion(binId, trackId, position, clipId, logUndo, refreshView, undo, redo)) {
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
    res = res && timeline->requestItemResize(newId, duration - newDuration, false, true, undo, redo);
    // parse effects
    std::shared_ptr<EffectStackModel> sourceStack = timeline->getClipEffectStackModel(clipId);
    sourceStack->cleanFadeEffects(true, undo, redo);
    std::shared_ptr<EffectStackModel> destStack = timeline->getClipEffectStackModel(newId);
    destStack->cleanFadeEffects(false, undo, redo);
    // The next requestclipmove does not check for duration change since we don't invalidate timeline, so check duration change now
    bool durationChanged = trackDuration != timeline->getTrackById_const(trackId)->trackDuration();
    res = res && timeline->requestClipMove(newId, trackId, position, true, false, undo, redo);
    if (durationChanged) {
        // Track length changed, check project duration
        Fun updateDuration  = [timeline]() {
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
    int count = 0;
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
        }
    }
    if (count > 0 && timeline->m_groups->isInGroup(clipId)) {
        // we now split the group hiearchy.
        // As a splitting criterion, we compare start point with split position
        auto criterion = [timeline, position](int cid) { return timeline->getClipPosition(cid) < position; };
        int root = timeline->m_groups->getRootId(clipId);
        bool res = timeline->m_groups->split(root, criterion, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    return count > 0;
}

int TimelineFunctions::requestSpacerStartOperation(std::shared_ptr<TimelineItemModel> timeline, int trackId, int position)
{
    std::unordered_set<int> clips = timeline->getItemsAfterPosition(trackId, position, -1);
    if (clips.size() > 0) {
        timeline->requestClipsGroup(clips, false);
        return (*clips.cbegin());
    }
    return -1;
}

bool TimelineFunctions::requestSpacerEndOperation(std::shared_ptr<TimelineItemModel> timeline, int clipId, int startPosition, int endPosition)
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
        if (clips.size() > 1) {
            final = timeline->requestGroupMove(clipId, res, 0, endPosition - startPosition, true, true, undo, redo);
        } else {
            // only 1 clip to be moved
            final = timeline->requestClipMove(clipId, track, endPosition, true, true, undo, redo);
        }
    }
    if (final && clips.size() > 1) {
        final = timeline->requestClipUngroup(clipId, undo, redo);
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
    bool result = false;
    for (int trackId : tracks) {
        result = TimelineFunctions::liftZone(timeline, trackId, zone, undo, redo);
        if (result && !liftOnly) {
            result = TimelineFunctions::removeSpace(timeline, trackId, zone, undo, redo);
        }
    }
    pCore->pushUndo(undo, redo, liftOnly ? i18n("Lift zone") : i18n("Extract zone"));
    return result;
}

bool TimelineFunctions::insertZone(std::shared_ptr<TimelineItemModel> timeline, int trackId, const QString &binId, int insertFrame, QPoint zone, bool overwrite)
{
    // Start undoable command
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool result = false;
    if (overwrite) {
        result = TimelineFunctions::liftZone(timeline, trackId, QPoint(insertFrame, insertFrame + (zone.y() - zone.x())), undo, redo);
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
    int newId = -1;
    QString binClipId = QString("%1/%2/%3").arg(binId).arg(zone.x()).arg(zone.y() - 1);
    timeline->requestClipInsertion(binClipId, trackId, insertFrame, newId, true, true, undo, redo);
    pCore->pushUndo(undo, redo, overwrite ? i18n("Overwrite zone") : i18n("Insert zone"));
    return result;
}

bool TimelineFunctions::liftZone(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo)
{
    // Check if there is a clip at start point
    int startClipId = timeline->getClipByPosition(trackId, zone.x());
    if (startClipId > -1) {
        // There is a clip, cut it
        if (timeline->getClipPosition(startClipId) < zone.x()) {
            TimelineFunctions::requestClipCut(timeline, startClipId, zone.x(), undo, redo);
        }
    }
    int endClipId = timeline->getClipByPosition(trackId, zone.y());
    if (endClipId > -1) {
        // There is a clip, cut it
        if (timeline->getClipPosition(endClipId) + timeline->getClipPlaytime(endClipId) > zone.y()) {
            TimelineFunctions::requestClipCut(timeline, endClipId, zone.y(), undo, redo);
        }
    }
    std::unordered_set<int> clips = timeline->getItemsAfterPosition(trackId, zone.x(), zone.y() - 1);
    for (const auto &clipId : clips) {
        timeline->requestItemDeletion(clipId, undo, redo);
    }
    return true;
}

bool TimelineFunctions::removeSpace(std::shared_ptr<TimelineItemModel> timeline, int trackId, QPoint zone, Fun &undo, Fun &redo)
{
    Q_UNUSED(trackId)

    std::unordered_set<int> clips = timeline->getItemsAfterPosition(-1, zone.y() - 1, -1, true);
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

    std::unordered_set<int> clips = timeline->getItemsAfterPosition(-1, zone.x(), -1, true);
    bool result = false;
    if (clips.size() > 0) {
        int clipId = *clips.begin();
        if (clips.size() > 1) {
            int res = timeline->requestClipsGroup(clips, undo, redo);
            if (res > -1) {
                result = timeline->requestGroupMove(clipId, res, 0, zone.y() - zone.x(), true, true, undo, redo);
                if (result) {
                    result = timeline->requestClipUngroup(clipId, undo, redo);
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
                QScopedPointer <Mlt::Properties> transProps(timeline->m_allCompositions[id]->properties());
                res = res & timeline->requestCompositionInsertion(transitionId, target_track, -1, target_position, timeline->m_allCompositions[id]->getPlaytime(), transProps.data(), newId, undo, redo);
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
    qDebug() << "Sucessful copy, coping groups...";
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
    timeline->dataChanged(modelIndex, modelIndex, {TimelineModel::KeyframesRole});
}

void TimelineFunctions::showCompositionKeyframes(std::shared_ptr<TimelineItemModel> timeline, int compoId, bool value)
{
    timeline->m_allCompositions[compoId]->setShowKeyframes(value);
    QModelIndex modelIndex = timeline->makeCompositionIndexFromID(compoId);
    timeline->dataChanged(modelIndex, modelIndex, {TimelineModel::KeyframesRole});
}

bool TimelineFunctions::changeClipState(std::shared_ptr<TimelineItemModel> timeline, int clipId, PlaylistState::ClipState status)
{
    PlaylistState::ClipState oldState = timeline->m_allClips[clipId]->clipState();
    if (oldState == status) {
        return false;
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool result = changeClipState(timeline, clipId, status, undo, redo);
    if (result) {
        pCore->pushUndo(undo, redo, i18n("Change clip state"));
    }
    return result;
}

bool TimelineFunctions::changeClipState(std::shared_ptr<TimelineItemModel> timeline, int clipId, PlaylistState::ClipState status, Fun &undo, Fun &redo)
{
    PlaylistState::ClipState oldState = timeline->m_allClips[clipId]->clipState();
    if (oldState == status) {
        return false;
    }
    Fun local_redo = [timeline, clipId, status]() {
        int trackId = timeline->getClipTrackId(clipId);
        bool res = timeline->m_allClips[clipId]->setClipState(status);
        // in order to make the producer change effective, we need to unplant / replant the clip in int track
        if (trackId != -1) {
            timeline->getTrackById(trackId)->replugClip(clipId);
            QModelIndex ix = timeline->makeClipIndexFromID(clipId);
            timeline->dataChanged(ix, ix, {TimelineModel::StatusRole});
            int start = timeline->getItemPosition(clipId);
            int end = start + timeline->getItemPlaytime(clipId);
            timeline->invalidateZone(start, end);
            timeline->checkRefresh(start, end);
        }
        return res;
    };
    Fun local_undo = [timeline, clipId, oldState]() {
        bool res = timeline->m_allClips[clipId]->setClipState(oldState);
        // in order to make the producer change effective, we need to unplant / replant the clip in int track
        int trackId = timeline->getClipTrackId(clipId);
        if (trackId != -1) {
            int start = timeline->getItemPosition(clipId);
            int end = start + timeline->getItemPlaytime(clipId);
            timeline->getTrackById(trackId)->replugClip(clipId);
            QModelIndex ix = timeline->makeClipIndexFromID(clipId);
            timeline->dataChanged(ix, ix, {TimelineModel::StatusRole});
            timeline->invalidateZone(start, end);
            timeline->checkRefresh(start, end);
        }
        return res;
    };
    bool result = local_redo();
    if (result) {
        PUSH_LAMBDA(local_undo, undo);
        PUSH_LAMBDA(local_redo, redo);
    }
    return result;
}

bool TimelineFunctions::requestSplitAudio(std::shared_ptr<TimelineItemModel> timeline, int clipId, int audioTarget)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    const std::unordered_set<int> clips = timeline->getGroupElements(clipId);
    for (int cid : clips) {
        int position = timeline->getClipPosition(cid);
        int track = timeline->getClipTrackId(cid);
        QList<int> possibleTracks = audioTarget >= 0 ? QList<int>() <<audioTarget : timeline->getLowerTracksId(track, TrackType::AudioTrack);
        if (possibleTracks.isEmpty()) {
            // No available audio track for splitting, abort
            undo();
            pCore->displayMessage(i18n("No available audio track for split operation"), ErrorMessage);
            return false;
        }
        int newId;
        bool res = copyClip(timeline, cid, newId, PlaylistState::AudioOnly, undo, redo);
        bool move = false;
        while (!move && !possibleTracks.isEmpty()) {
            int newTrack = possibleTracks.takeFirst();
            move = timeline->requestClipMove(newId, newTrack, position, true, false, undo, redo);
        }
        std::unordered_set<int> groupClips;
        groupClips.insert(cid);
        groupClips.insert(newId);
        if (!res || !move) {
            bool undone = undo();
            Q_ASSERT(undone);
            pCore->displayMessage(i18n("Audio split failed"), ErrorMessage);
            return false;
        }
        TimelineFunctions::changeClipState(timeline, cid, PlaylistState::VideoOnly, undo, redo);
        timeline->requestClipsGroup(groupClips, undo, redo, GroupType::AVSplit);
    }
    pCore->pushUndo(undo, redo, i18n("Split Audio"));
    return true;
}

void TimelineFunctions::setCompositionATrack(std::shared_ptr<TimelineItemModel> timeline, int cid, int aTrack)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    std::shared_ptr<CompositionModel>compo = timeline->getCompositionPtr(cid);
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
        timeline->getCompositionPtr(cid)->setATrack(previousATrack, previousATrack<= 0 ? -1 : timeline->getTrackIndexFromPosition(previousATrack - 1));
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
