/*
SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelinefunctions.hpp"
#include "bin/bin.h"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/subtitlemodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "clipmodel.hpp"
#include "compositionmodel.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "groupsmodel.hpp"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "project/projectmanager.h"
#include "timelineitemmodel.hpp"
#include "trackmodel.hpp"
#include "transitions/transitionsrepository.hpp"

#include "utils/KMessageBox_KdenliveCompat.h"
#include <KIO/RenameDialog>
#include <KLocalizedString>
#include <KMessageBox>
#include <QApplication>
#include <QDebug>
#include <QInputDialog>
#include <QSemaphore>
#include <unordered_map>

#ifdef CRASH_AUTO_TEST
#include "logger.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <rttr/registration>
#pragma GCC diagnostic pop

RTTR_REGISTRATION
{
    using namespace rttr;
    registration::class_<TimelineFunctions>("TimelineFunctions")
        .method("requestClipCut", select_overload<bool(std::shared_ptr<TimelineItemModel>, int, int)>(&TimelineFunctions::requestClipCut))(
            parameter_names("timeline", "clipId", "position"))
        .method("requestDeleteBlankAt", select_overload<bool(const std::shared_ptr<TimelineItemModel> &, int, int, bool)>(
                                            &TimelineFunctions::requestDeleteBlankAt))(parameter_names("timeline", "trackId", "position", "affectAllTracks"));
}
#else
#define TRACE_STATIC(...)
#define TRACE_RES(...)
#endif

QStringList waitingBinIds;
QMap<QString, QString> mappedIds;
QMap<int, int> tracksMap;
QMap<int, int> spacerUngroupedItems;
int spacerMinPosition;
QSemaphore semaphore(1);

bool TimelineFunctions::cloneClip(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int &newId, PlaylistState::ClipState state, Fun &undo,
                                  Fun &redo)
{
    // Special case: slowmotion clips
    double clipSpeed = timeline->m_allClips[clipId]->getSpeed();
    bool warp_pitch = timeline->m_allClips[clipId]->getIntProperty(QStringLiteral("warp_pitch"));
    int audioStream = timeline->m_allClips[clipId]->getIntProperty(QStringLiteral("audio_index"));
    bool res = timeline->requestClipCreation(timeline->getClipBinId(clipId), newId, state, audioStream, clipSpeed, warp_pitch, undo, redo);
    timeline->m_allClips[newId]->m_endlessResize = timeline->m_allClips[clipId]->m_endlessResize;

    // copy useful timeline properties
    timeline->m_allClips[clipId]->passTimelineProperties(timeline->m_allClips[newId]);

    int duration = timeline->getClipPlaytime(clipId);
    int init_duration = timeline->getClipPlaytime(newId);
    if (duration != init_duration) {
        init_duration -= timeline->m_allClips[clipId]->getIn();
        res = res && timeline->requestItemResize(newId, init_duration, false, true, undo, redo);
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

bool TimelineFunctions::requestMultipleClipsInsertion(const std::shared_ptr<TimelineItemModel> &timeline, const QStringList &binIds, int trackId, int position,
                                                      QList<int> &clipIds, bool logUndo, bool refreshView)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    for (const QString &binId : binIds) {
        int clipId;
        if (timeline->requestClipInsertion(binId, trackId, position, clipId, logUndo, refreshView, false, undo, redo)) {
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

bool TimelineFunctions::processClipCut(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int position, int &newId, Fun &undo, Fun &redo)
{
    bool isSubtitle = timeline->isSubTitle(clipId);
    int trackId = isSubtitle ? -1 : timeline->getClipTrackId(clipId);
    int trackDuration = isSubtitle ? -1 : timeline->getTrackById_const(trackId)->trackDuration();
    int start = timeline->getItemPosition(clipId);
    int duration = timeline->getItemPlaytime(clipId);
    if (start > position || (start + duration) < position) {
        return false;
    }
    if (isSubtitle) {
        newId = timeline->cutSubtitle(position, undo, redo);
        return newId > -1;
    }
    bool hasEndMix = timeline->getTrackById_const(trackId)->hasEndMix(clipId);
    bool hasStartMix = timeline->getTrackById_const(trackId)->hasStartMix(clipId);
    int subplaylist = timeline->m_allClips[clipId]->getSubPlaylistIndex();
    PlaylistState::ClipState state = timeline->m_allClips[clipId]->clipState();
    // Check if clip has an end Mix
    bool res = cloneClip(timeline, clipId, newId, state, undo, redo);
    timeline->m_blockRefresh = true;

    int updatedDuration = position - start;
    // Resize original clip
    res = timeline->m_allClips[clipId]->requestResize(updatedDuration, true, undo, redo, true, hasEndMix || hasStartMix);

    if (hasEndMix) {
        // Assing end mix to new clone clip
        Fun local_redo = [timeline, trackId, clipId, newId]() { return timeline->getTrackById_const(trackId)->reAssignEndMix(clipId, newId); };
        local_redo();
        PUSH_LAMBDA(local_redo, redo);
        // Reassing end mix to original clip on undo
        Fun local_undo = [timeline, trackId, clipId, newId]() {
            timeline->getTrackById_const(trackId)->reAssignEndMix(newId, clipId);
            return true;
        };
        PUSH_LAMBDA(local_undo, undo);
        // Assing end mix to new clone clip
        if (!hasStartMix && subplaylist != 1) {
            Fun local_redo2 = [timeline, trackId, clipId, start]() {
                // If the clip has no start mix, move to playlist 1
                return timeline->getTrackById_const(trackId)->switchPlaylist(clipId, start, 0, 1);
            };
            // Restore initial subplaylist on undo
            Fun local_undo2 = [timeline, trackId, clipId, start]() {
                // If the clip has no start mix, move back to playlist 0
                return timeline->getTrackById_const(trackId)->switchPlaylist(clipId, start, 1, 0);
            };
            res = res && local_redo2();
            if (res) {
                UPDATE_UNDO_REDO_NOLOCK(local_redo2, local_undo2, undo, redo);
            }
        }
    }
    int newDuration = timeline->getClipPlaytime(clipId);
    // parse effects
    if (res) {
        std::shared_ptr<EffectStackModel> sourceStack = timeline->getClipEffectStackModel(clipId);
        sourceStack->cleanFadeEffects(true, undo, redo);
        std::shared_ptr<EffectStackModel> destStack = timeline->getClipEffectStackModel(newId);
        destStack->cleanFadeEffects(false, undo, redo);
    }
    updatedDuration = duration - newDuration;
    res = res && timeline->requestItemResize(newId, updatedDuration, false, true, undo, redo);
    // The next requestclipmove does not check for duration change since we don't invalidate timeline, so check duration change now
    bool durationChanged = trackDuration != timeline->getTrackById_const(trackId)->trackDuration();
    if (hasEndMix) {
        timeline->m_allClips[newId]->setSubPlaylistIndex(subplaylist, trackId);
    }
    res = res && timeline->requestClipMove(newId, trackId, position, true, true, false, true, undo, redo);

    if (durationChanged) {
        // Track length changed, check project duration
        Fun updateDuration = [timeline]() {
            timeline->updateDuration();
            return true;
        };
        updateDuration();
        PUSH_LAMBDA(updateDuration, redo);
    }
    timeline->m_blockRefresh = false;
    return res;
}

bool TimelineFunctions::requestClipCut(std::shared_ptr<TimelineItemModel> timeline, int clipId, int position)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    TRACE_STATIC(timeline, clipId, position);
    bool result = TimelineFunctions::requestClipCut(timeline, clipId, position, undo, redo);
    if (result) {
        pCore->pushUndo(undo, redo, i18n("Cut clip"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineFunctions::requestClipCut(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int position, Fun &undo, Fun &redo)
{
    const std::unordered_set<int> clipselect = timeline->getGroupElements(clipId);
    // Remove locked items
    std::unordered_set<int> clips;
    for (int cid : clipselect) {
        if (timeline->isSubTitle(cid)) {
            clips.insert(cid);
            continue;
        }
        if (!timeline->isClip(cid)) {
            continue;
        }
        int tk = timeline->getClipTrackId(cid);
        if (tk != -1 && !timeline->getTrackById_const(tk)->isLocked()) {
            clips.insert(cid);
        }
    }
    // Shall we reselect after the split
    int trackToSelect = -1;
    if (timeline->isClip(clipId) && timeline->m_allClips[clipId]->selected) {
        int mainIn = timeline->getItemPosition(clipId);
        int mainOut = mainIn + timeline->getItemPlaytime(clipId);
        if (position > mainIn && position < mainOut) {
            trackToSelect = timeline->getItemTrackId(clipId);
        }
    }
    // We need to call clearSelection before attempting the split or the group split will be corrupted by the selection group (no undo support)
    timeline->requestClearSelection();

    std::unordered_set<int> topElements;
    std::transform(clips.begin(), clips.end(), std::inserter(topElements, topElements.begin()), [&](int id) { return timeline->m_groups->getRootId(id); });

    int count = 0;
    QList<int> newIds;
    QList<int> clipsToCut;
    bool subtitleItemSelected = false;
    for (int cid : clips) {
        if (!timeline->isClip(cid) && !timeline->isSubTitle(cid)) {
            continue;
        }
        int start = timeline->getItemPosition(cid);
        int duration = timeline->getItemPlaytime(cid);
        if (start < position && (start + duration) > position) {
            clipsToCut << cid;
            if (timeline->isSubTitle(cid)) {
                if (subtitleItemSelected) {
                    // We cannot cut 2 overlapping subtitles at the same position
                    pCore->displayMessage(i18nc("@info:status", "Cannot cut overlapping subtitles"), ErrorMessage, 500);
                    bool undone = undo();
                    Q_ASSERT(undone);
                    return false;
                }
                subtitleItemSelected = true;
            }
        }
    }
    if (clipsToCut.isEmpty()) {
        return true;
    }

    for (int cid : qAsConst(clipsToCut)) {
        count++;
        int newId = -1;
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
    if (count > 0 && timeline->m_groups->isInGroup(clipId)) {
        // we now split the group hierarchy.
        // As a splitting criterion, we compare start point with split position
        auto criterion = [timeline, position](int cid) { return timeline->getItemPosition(cid) < position; };
        bool res = true;
        for (const int topId : topElements) {
            qDebug() << "// CHECKING REGROUP ELEMENT: " << topId << ", ISCLIP: " << timeline->isClip(topId) << timeline->isGroup(topId);
            res = res && timeline->m_groups->split(topId, criterion, undo, redo);
        }
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    if (count > 0 && trackToSelect > -1) {
        int newClip = timeline->getClipByPosition(trackToSelect, position);
        if (newClip > -1) {
            timeline->requestSetSelection({newClip});
        }
    }
    return count > 0;
}

bool TimelineFunctions::requestClipCutAll(std::shared_ptr<TimelineItemModel> timeline, int position)
{
    QVector<std::shared_ptr<TrackModel>> affectedTracks;
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };

    for (const auto &track : timeline->m_allTracks) {
        if (track->shouldReceiveTimelineOp()) {
            affectedTracks << track;
        }
    }

    unsigned count = 0;
    auto subModel = timeline->getSubtitleModel();
    if (subModel && !subModel->isLocked()) {
        int clipId = timeline->getClipByPosition(-2, position);
        if (clipId > -1) {
            // Found subtitle clip at position in track, cut it. Update undo/redo as we go.
            if (!TimelineFunctions::requestClipCut(timeline, clipId, position, undo, redo)) {
                qWarning() << "Failed to cut clip " << clipId << " at " << position;
                pCore->displayMessage(i18n("Failed to cut clip"), ErrorMessage, 500);
                // Undo all cuts made, assert successful undo.
                bool undone = undo();
                Q_ASSERT(undone);
                return false;
            }
            count++;
        }
    }
    if (affectedTracks.isEmpty() && count == 0) {
        pCore->displayMessage(i18n("All tracks are locked"), ErrorMessage, 500);
        return false;
    }
    for (auto track : qAsConst(affectedTracks)) {
        int clipId = track->getClipByPosition(position);
        if (clipId > -1) {
            // Found clip at position in track, cut it. Update undo/redo as we go.
            if (!TimelineFunctions::requestClipCut(timeline, clipId, position, undo, redo)) {
                qWarning() << "Failed to cut clip " << clipId << " at " << position;
                pCore->displayMessage(i18n("Failed to cut clip"), ErrorMessage, 500);
                // Undo all cuts made, assert successful undo.
                bool undone = undo();
                Q_ASSERT(undone);
                return false;
            }
            count++;
        }
    }

    if (!count) {
        pCore->displayMessage(i18n("No clips to cut"), ErrorMessage);
    } else {
        pCore->pushUndo(undo, redo, i18n("Cut all clips"));
    }

    return count > 0;
}

std::pair<int, int> TimelineFunctions::requestSpacerStartOperation(const std::shared_ptr<TimelineItemModel> &timeline, int trackId, int position,
                                                                   bool ignoreMultiTrackGroups, bool allowGroupBreaking)
{
    if (trackId != -1 && timeline->trackIsLocked(trackId)) {
        timeline->flashLock(trackId);
        return {-1, -1};
    }
    std::unordered_set<int> clips = timeline->getItemsInRange(trackId, position, -1);
    timeline->requestClearSelection();
    // Find the first clip on each track to calculate the minimum space operation
    QMap<int, int> firstClipOnTrack;
    // Find the maximum space allowed by grouped clips placed before the operation start {trackid,blank_duration}
    QMap<int, int> relatedMaxSpace;
    spacerMinPosition = -1;
    if (!clips.empty()) {
        // Remove grouped items that are before the click position
        // First get top groups ids
        std::unordered_set<int> roots;
        spacerUngroupedItems.clear();
        std::transform(clips.begin(), clips.end(), std::inserter(roots, roots.begin()), [&](int id) { return timeline->m_groups->getRootId(id); });
        std::unordered_set<int> groupsToRemove;
        int firstCid = -1;
        int spaceDuration = -1;
        std::unordered_set<int> toSelect;
        //  List all clips involved in the spacer operation
        std::unordered_set<int> allClips;
        for (int r : roots) {
            std::unordered_set<int> children = timeline->m_groups->getLeaves(r);
            allClips.insert(children.begin(), children.end());
        }
        for (int r : roots) {
            if (timeline->isGroup(r)) {
                std::unordered_set<int> leaves = timeline->m_groups->getLeaves(r);
                std::unordered_set<int> leavesToRemove;
                std::unordered_set<int> leavesToKeep;
                for (int l : leaves) {
                    int pos = timeline->getItemPosition(l);
                    bool outOfRange = timeline->getItemEnd(l) < position;
                    int tid = timeline->getItemTrackId(l);
                    bool unaffectedTrack = ignoreMultiTrackGroups && trackId > -1 && tid != trackId;
                    if (allowGroupBreaking) {
                        if (outOfRange || unaffectedTrack) {
                            leavesToRemove.insert(l);
                        } else {
                            leavesToKeep.insert(l);
                        }
                    } else if (outOfRange) {
                        // This is a grouped clip positionned before the spacer operation position, check maximum space before
                        std::unordered_set<int> beforeOnTrack = timeline->getItemsInRange(tid, 0, pos - 1);
                        for (auto &c : allClips) {
                            beforeOnTrack.erase(c);
                        }
                        int lastPos = 0;
                        for (int c : beforeOnTrack) {
                            int p = timeline->getItemEnd(c);
                            if (p >= pos - 1) {
                                lastPos = pos;
                                break;
                            }
                            if (p > lastPos) {
                                lastPos = p;
                            }
                        }
                        if (relatedMaxSpace.contains(trackId)) {
                            if (relatedMaxSpace.value(trackId) > (pos - lastPos)) {
                                relatedMaxSpace.insert(trackId, pos - lastPos);
                            }
                        } else {
                            relatedMaxSpace.insert(trackId, pos - lastPos);
                        }
                    }
                    if (!outOfRange && !unaffectedTrack) {
                        // Find first item
                        if (!firstClipOnTrack.contains(tid)) {
                            firstClipOnTrack.insert(tid, l);
                        } else if (timeline->getItemPosition(firstClipOnTrack.value(tid)) > pos) {
                            firstClipOnTrack.insert(tid, l);
                        }
                    }
                }
                for (int l : leavesToRemove) {
                    int checkedParent = timeline->m_groups->getDirectAncestor(l);
                    if (checkedParent < 0) {
                        checkedParent = l;
                    }
                    spacerUngroupedItems.insert(l, checkedParent);
                }
                if (leavesToKeep.size() == 1) {
                    toSelect.insert(*leavesToKeep.begin());
                    groupsToRemove.insert(r);
                }
            } else {
                // Find first clip on track
                int pos = timeline->getItemPosition(r);
                int tid = timeline->getItemTrackId(r);
                if (!firstClipOnTrack.contains(tid)) {
                    firstClipOnTrack.insert(tid, r);
                } else if (timeline->getItemPosition(firstClipOnTrack.value(tid)) > pos) {
                    firstClipOnTrack.insert(tid, r);
                }
            }
        }
        toSelect.insert(roots.begin(), roots.end());
        for (int r : groupsToRemove) {
            toSelect.erase(r);
        }

        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        QMapIterator<int, int> i(spacerUngroupedItems);
        while (i.hasNext()) {
            i.next();
            timeline->m_groups->removeFromGroup(i.key());
        }

        timeline->requestSetSelection(toSelect);

        QMapIterator<int, int> it(firstClipOnTrack);
        int firstPos = -1;
        if (firstClipOnTrack.isEmpty() && firstCid > -1) {
            int clipPos = timeline->getItemPosition(firstCid);
            spaceDuration = timeline->getTrackById_const(timeline->getItemTrackId(firstCid))->getBlankSizeAtPos(clipPos - 1);
        }
        while (it.hasNext()) {
            it.next();
            int clipPos = timeline->getItemPosition(it.value());
            if (trackId > -1) {
                if (it.key() == trackId) {
                    firstCid = it.value();
                }
            } else {
                if (firstPos == -1) {
                    firstCid = it.value();
                    firstPos = clipPos;
                } else if (firstPos < clipPos) {
                    firstCid = it.value();
                }
            }
            if (timeline->isSubtitleTrack(it.key())) {
                if (timeline->getSubtitleModel()->isBlankAt(clipPos - 1)) {
                    if (spaceDuration == -1) {
                        spaceDuration = timeline->getSubtitleModel()->getBlankSizeAtPos(clipPos - 1);
                    } else {
                        int blank = timeline->getSubtitleModel()->getBlankSizeAtPos(clipPos - 1);
                        spaceDuration = qMin(blank, spaceDuration);
                    }
                }
            } else {
                if (timeline->getTrackById_const(it.key())->isBlankAt(clipPos - 1)) {
                    if (spaceDuration == -1) {
                        spaceDuration = timeline->getTrackById_const(it.key())->getBlankSizeAtPos(clipPos - 1);
                    } else {
                        int blank = timeline->getTrackById_const(it.key())->getBlankSizeAtPos(clipPos - 1);
                        spaceDuration = qMin(blank, spaceDuration);
                    }
                }
            }
            if (relatedMaxSpace.contains(it.key())) {
                spaceDuration = qMin(spaceDuration, relatedMaxSpace.value(it.key()));
            }
        }
        spacerMinPosition = timeline->getItemPosition(firstCid) - spaceDuration;
        return {firstCid, spaceDuration};
    }
    return {-1, -1};
}

bool TimelineFunctions::requestSpacerEndOperation(const std::shared_ptr<TimelineItemModel> &timeline, int itemId, int startPosition, int endPosition,
                                                  int affectedTrack, int moveGuidesPosition, Fun &undo, Fun &redo, bool pushUndo)
{
    // Move guides if needed
    if (moveGuidesPosition > -1) {
        moveGuidesPosition = qMin(moveGuidesPosition, startPosition);
        GenTime fromPos(moveGuidesPosition, pCore->getCurrentFps());
        GenTime toPos(endPosition - startPosition, pCore->getCurrentFps());
        QList<CommentedTime> guides = timeline->getGuideModel()->getMarkersInRange(moveGuidesPosition, -1);
        if (!guides.isEmpty()) {
            timeline->getGuideModel()->moveMarkers(guides, fromPos, fromPos + toPos, undo, redo);
        }
    }

    // Move group back to original position
    spacerMinPosition = -1;
    int track = timeline->getItemTrackId(itemId);
    bool isClip = timeline->isClip(itemId);
    if (isClip) {
        timeline->requestClipMove(itemId, track, startPosition, true, false, false, false, true);
    } else if (timeline->isComposition(itemId)) {
        timeline->requestCompositionMove(itemId, track, startPosition, false, false);
    } else {
        timeline->requestSubtitleMove(itemId, startPosition, false, false);
    }

    std::unordered_set<int> clips = timeline->getGroupElements(itemId);
    int mainGroup = timeline->m_groups->getRootId(itemId);
    bool final = false;
    bool liftOk = true;
    if (timeline->m_editMode == TimelineMode::OverwriteEdit && endPosition < startPosition) {
        // Remove zone between end and start pos
        if (affectedTrack == -1) {
            // touch all tracks
            auto it = timeline->m_allTracks.cbegin();
            while (it != timeline->m_allTracks.cend()) {
                int target_track = (*it)->getId();
                if (!timeline->getTrackById_const(target_track)->isLocked()) {
                    liftOk = liftOk && TimelineFunctions::liftZone(timeline, target_track, QPoint(endPosition, startPosition), undo, redo);
                }
                ++it;
            }
        } else if (timeline->isTrack(affectedTrack)) {
            liftOk = TimelineFunctions::liftZone(timeline, affectedTrack, QPoint(endPosition, startPosition), undo, redo);
        }
        // The lift operation destroys selection group, so regroup now
        if (clips.size() > 1) {
            timeline->requestSetSelection(clips);
            mainGroup = timeline->m_groups->getRootId(itemId);
        }
    }
    if (liftOk && (mainGroup > -1 || clips.size() == 1)) {
        if (clips.size() > 1) {
            final = timeline->requestGroupMove(itemId, mainGroup, 0, endPosition - startPosition, true, true, undo, redo);
        } else {
            // only 1 clip to be moved
            if (isClip) {
                final = timeline->requestClipMove(itemId, track, endPosition, true, true, true, true, undo, redo);
            } else if (timeline->isComposition(itemId)) {
                final = timeline->requestCompositionMove(itemId, track, -1, endPosition, true, true, undo, redo);
            } else {
                final = timeline->requestSubtitleMove(itemId, endPosition, true, true, true, true, undo, redo);
            }
        }
    }
    timeline->requestClearSelection();
    if (final) {
        if (pushUndo) {
            if (startPosition < endPosition) {
                pCore->pushUndo(undo, redo, i18n("Insert space"));
            } else {
                pCore->pushUndo(undo, redo, i18n("Remove space"));
            }
        }
        // Regroup temporarily ungrouped items
        QMapIterator<int, int> i(spacerUngroupedItems);
        Fun local_undo = []() { return true; };
        Fun local_redo = []() { return true; };
        std::unordered_set<int> newlyGrouped;
        while (i.hasNext()) {
            i.next();
            if (timeline->isItem(i.value())) {
                if (newlyGrouped.count(i.value()) > 0) {
                    Q_ASSERT(timeline->m_groups->isInGroup(i.value()));
                    timeline->m_groups->setInGroupOf(i.key(), i.value(), local_undo, local_redo);
                } else {
                    std::unordered_set<int> items = {i.key(), i.value()};
                    timeline->m_groups->groupItems(items, local_undo, local_redo);
                    newlyGrouped.insert(i.value());
                }
            } else {
                // i.value() is either a group (detectable via timeline->isGroup) or an empty group
                if (timeline->isGroup(i.key())) {
                    std::unordered_set<int> items = {i.key(), i.value()};
                    timeline->m_groups->groupItems(items, local_undo, local_redo);
                } else {
                    timeline->m_groups->setGroup(i.key(), i.value());
                }
            }
        }
        spacerUngroupedItems.clear();
        return true;
    } else {
        undo();
    }
    return false;
}

bool TimelineFunctions::breakAffectedGroups(const std::shared_ptr<TimelineItemModel> &timeline, const QVector<int> &tracks, QPoint zone, Fun &undo, Fun &redo)
{
    // Check if we have grouped clips that are on unaffected tracks, and ungroup them
    bool result = true;
    std::unordered_set<int> affectedItems;
    // First find all affected items
    for (auto trackId : tracks) {
        std::unordered_set<int> items = timeline->getItemsInRange(trackId, zone.x(), zone.y());
        affectedItems.insert(items.begin(), items.end());
    }
    for (int item : affectedItems) {
        if (timeline->m_groups->isInGroup(item)) {
            int groupId = timeline->m_groups->getRootId(item);
            std::unordered_set<int> all_children = timeline->m_groups->getLeaves(groupId);
            for (int child : all_children) {
                int childTrackId = timeline->getItemTrackId(child);
                if (!tracks.contains(childTrackId) && timeline->m_groups->isInGroup(child)) {
                    // This item should not be affected by the operation, ungroup it
                    result = result && timeline->requestClipUngroup(child, undo, redo);
                }
            }
        }
    }
    return result;
}

bool TimelineFunctions::extractZone(const std::shared_ptr<TimelineItemModel> &timeline, const QVector<int> &tracks, QPoint zone, bool liftOnly,
                                    int clipToUnGroup, std::unordered_set<int> clipsToRegroup)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = extractZoneWithUndo(timeline, tracks, zone, liftOnly, clipToUnGroup, clipsToRegroup, undo, redo);
    pCore->pushUndo(undo, redo, liftOnly ? i18n("Lift zone") : i18n("Extract zone"));
    return res;
}

bool TimelineFunctions::extractZoneWithUndo(const std::shared_ptr<TimelineItemModel> &timeline, const QVector<int> &tracks, QPoint zone, bool liftOnly,
                                            int clipToUnGroup, std::unordered_set<int> clipsToRegroup, Fun &undo, Fun &redo)
{
    // Start undoable command
    bool result = true;
    if (clipToUnGroup > -1) {
        result = timeline->requestClipUngroup(clipToUnGroup, undo, redo);
    }
    result = breakAffectedGroups(timeline, tracks, zone, undo, redo);
    for (auto trackId : tracks) {
        if (timeline->getTrackById_const(trackId)->isLocked()) {
            continue;
        }
        result = result && TimelineFunctions::liftZone(timeline, trackId, zone, undo, redo);
    }
    if (result && !liftOnly) {
        result = TimelineFunctions::removeSpace(timeline, zone, undo, redo, tracks);
    }
    if (clipsToRegroup.size() > 1) {
        result = timeline->requestClipsGroup(clipsToRegroup, undo, redo);
    }
    return result;
}

bool TimelineFunctions::insertZone(const std::shared_ptr<TimelineItemModel> &timeline, const QList<int> &trackIds, const QString &binId, int insertFrame,
                                   QPoint zone, bool overwrite, bool useTargets)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = TimelineFunctions::insertZone(timeline, trackIds, binId, insertFrame, zone, overwrite, useTargets, undo, redo);
    if (res) {
        pCore->pushUndo(undo, redo, overwrite ? i18n("Overwrite zone") : i18n("Insert zone"));
    } else {
        pCore->displayMessage(i18n("Could not insert zone"), ErrorMessage);
        undo();
    }
    return res;
}

bool TimelineFunctions::insertZone(const std::shared_ptr<TimelineItemModel> &timeline, QList<int> trackIds, const QString &binId, int insertFrame, QPoint zone,
                                   bool overwrite, bool useTargets, Fun &undo, Fun &redo)
{
    // Start undoable command
    bool result = true;
    QVector<int> affectedTracks;
    auto it = timeline->m_allTracks.cbegin();
    if (!useTargets) {
        // Timeline drop in overwrite mode
        for (int target_track : trackIds) {
            if (!timeline->getTrackById_const(target_track)->isLocked()) {
                affectedTracks << target_track;
            }
        }
    } else {
        while (it != timeline->m_allTracks.cend()) {
            int target_track = (*it)->getId();
            if (timeline->getTrackById_const(target_track)->shouldReceiveTimelineOp()) {
                affectedTracks << target_track;
            } else if (trackIds.contains(target_track)) {
                // Track is marked as target but not active, remove it
                trackIds.removeAll(target_track);
            }
            ++it;
        }
    }
    if (affectedTracks.isEmpty()) {
        pCore->displayMessage(i18n("Please activate a track by clicking on a track's label"), ErrorMessage);
        return false;
    }
    result = breakAffectedGroups(timeline, affectedTracks, QPoint(insertFrame, insertFrame + (zone.y() - zone.x())), undo, redo);
    if (overwrite) {
        // Cut all tracks
        for (int target_track : qAsConst(affectedTracks)) {
            result = result && TimelineFunctions::liftZone(timeline, target_track, QPoint(insertFrame, insertFrame + (zone.y() - zone.x())), undo, redo);
            if (!result) {
                qDebug() << "// LIFTING ZONE FAILED\n";
                break;
            }
        }
    } else {
        // Cut all tracks
        for (int target_track : qAsConst(affectedTracks)) {
            int startClipId = timeline->getClipByPosition(target_track, insertFrame);
            if (startClipId > -1) {
                // There is a clip, cut it
                result = result && TimelineFunctions::requestClipCut(timeline, startClipId, insertFrame, undo, redo);
            }
        }
        result =
            result && TimelineFunctions::requestInsertSpace(timeline, QPoint(insertFrame, insertFrame + (zone.y() - zone.x())), undo, redo, affectedTracks);
    }
    if (result) {
        if (!trackIds.isEmpty()) {
            int newId = -1;
            QString binClipId;
            if (binId.contains(QLatin1Char('/'))) {
                binClipId = QString("%1/%2/%3").arg(binId.section(QLatin1Char('/'), 0, 0)).arg(zone.x()).arg(zone.y() - 1);
            } else {
                binClipId = QString("%1/%2/%3").arg(binId).arg(zone.x()).arg(zone.y() - 1);
            }
            result = timeline->requestClipInsertion(binClipId, trackIds.first(), insertFrame, newId, true, true, useTargets, undo, redo, affectedTracks);
        }
    }
    return result;
}

bool TimelineFunctions::liftZone(const std::shared_ptr<TimelineItemModel> &timeline, int trackId, QPoint zone, Fun &undo, Fun &redo)
{
    // Check if there is a clip at start point
    int startClipId = timeline->getClipByPosition(trackId, zone.x());
    if (startClipId > -1) {
        // There is a clip, cut it
        if (timeline->getClipPosition(startClipId) < zone.x()) {
            // Check if we have a mix
            std::pair<MixInfo, MixInfo> mixData = timeline->getTrackById_const(trackId)->getMixInfo(startClipId);
            bool abortCut = false;
            if (mixData.first.firstClipId > -1) {
                // Clip has a start mix
                if (mixData.first.secondClipInOut.first + (mixData.first.firstClipInOut.second - mixData.first.secondClipInOut.first) -
                        mixData.first.mixOffset >=
                    zone.x()) {
                    // Cut pos is in the mix zone before clip cut, completely remove clip
                    abortCut = true;
                }
            }
            if (!abortCut) {
                TimelineFunctions::requestClipCut(timeline, startClipId, zone.x(), undo, redo);
            } else {
                // Remove the clip now, so that the mix is deleted before checking items in range
                timeline->requestClipUngroup(startClipId, undo, redo);
                timeline->requestItemDeletion(startClipId, undo, redo);
            }
        }
    }
    int endClipId = timeline->getClipByPosition(trackId, zone.y());
    if (endClipId > -1) {
        // There is a clip, cut it
        if (timeline->getClipPosition(endClipId) + timeline->getClipPlaytime(endClipId) > zone.y()) {
            // Check if we have a mix
            std::pair<MixInfo, MixInfo> mixData = timeline->getTrackById_const(trackId)->getMixInfo(endClipId);
            bool abortCut = false;
            if (mixData.second.firstClipId > -1) {
                // Clip has an end mix
                if (mixData.second.firstClipInOut.second - (mixData.second.firstClipInOut.second - mixData.second.secondClipInOut.first) -
                        mixData.first.mixOffset <=
                    zone.y()) {
                    // Cut pos is in the mix zone after clip cut, completely remove clip
                    abortCut = true;
                }
            }
            if (!abortCut) {
                TimelineFunctions::requestClipCut(timeline, endClipId, zone.y(), undo, redo);
            } else {
                // Remove the clip now, so that the mix is deleted before checking items in range
                timeline->requestClipUngroup(endClipId, undo, redo);
                timeline->requestItemDeletion(endClipId, undo, redo);
            }
        }
    }
    std::unordered_set<int> clips = timeline->getItemsInRange(trackId, zone.x(), zone.y());
    for (const auto &clipId : clips) {
        timeline->requestClipUngroup(clipId, undo, redo);
        timeline->requestItemDeletion(clipId, undo, redo);
    }
    return true;
}

bool TimelineFunctions::removeSpace(const std::shared_ptr<TimelineItemModel> &timeline, QPoint zone, Fun &undo, Fun &redo, const QVector<int> &allowedTracks,
                                    bool useTargets)
{
    std::unordered_set<int> clips;
    if (useTargets) {
        auto it = timeline->m_allTracks.cbegin();
        while (it != timeline->m_allTracks.cend()) {
            int target_track = (*it)->getId();
            if (timeline->getTrackById_const(target_track)->shouldReceiveTimelineOp()) {
                std::unordered_set<int> subs = timeline->getItemsInRange(target_track, zone.y() - 1, -1, true);
                clips.insert(subs.begin(), subs.end());
            }
            ++it;
        }
    } else {
        for (auto tid : allowedTracks) {
            std::unordered_set<int> subs = timeline->getItemsInRange(tid, zone.y() - 1, -1, true);
            clips.insert(subs.begin(), subs.end());
        }
    }
    if (clips.size() == 0) {
        // TODO: inform user no change will be performed
        pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
        return true;
    }
    bool result = false;
    timeline->requestSetSelection(clips);
    int itemId = *clips.begin();
    int targetTrackId = timeline->getItemTrackId(itemId);
    int targetPos = timeline->getItemPosition(itemId) + zone.x() - zone.y();

    if (timeline->m_groups->isInGroup(itemId)) {
        result = timeline->requestGroupMove(itemId, timeline->m_groups->getRootId(itemId), 0, zone.x() - zone.y(), true, true, undo, redo, true, true, true,
                                            allowedTracks);
    } else if (timeline->isClip(itemId)) {
        result = timeline->requestClipMove(itemId, targetTrackId, targetPos, true, true, true, true, undo, redo);
    } else {
        result =
            timeline->requestCompositionMove(itemId, targetTrackId, timeline->m_allCompositions[itemId]->getForcedTrack(), targetPos, true, true, undo, redo);
    }
    timeline->requestClearSelection();
    if (!result) {
        undo();
    }
    return result;
}

bool TimelineFunctions::requestInsertSpace(const std::shared_ptr<TimelineItemModel> &timeline, QPoint zone, Fun &undo, Fun &redo,
                                           const QVector<int> &allowedTracks)
{
    timeline->requestClearSelection();
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    std::unordered_set<int> items;
    if (allowedTracks.isEmpty()) {
        // Select clips in all tracks
        items = timeline->getItemsInRange(-1, zone.x(), -1, true);
    } else {
        // Select clips in target and active tracks only
        for (int target_track : allowedTracks) {
            std::unordered_set<int> subs = timeline->getItemsInRange(target_track, zone.x(), -1, true);
            items.insert(subs.begin(), subs.end());
        }
    }
    if (items.empty()) {
        return true;
    }
    timeline->requestSetSelection(items);
    bool result = true;
    int itemId = *(items.begin());
    int targetTrackId = timeline->getItemTrackId(itemId);
    int targetPos = timeline->getItemPosition(itemId) + zone.y() - zone.x();

    // TODO the three move functions should be unified in a "requestItemMove" function
    if (timeline->m_groups->isInGroup(itemId)) {
        result = result && timeline->requestGroupMove(itemId, timeline->m_groups->getRootId(itemId), 0, zone.y() - zone.x(), true, true, local_undo, local_redo,
                                                      true, true, true, allowedTracks);
    } else if (timeline->isClip(itemId)) {
        result = result && timeline->requestClipMove(itemId, targetTrackId, targetPos, true, true, true, true, local_undo, local_redo);
    } else {
        result = result && timeline->requestCompositionMove(itemId, targetTrackId, timeline->m_allCompositions[itemId]->getForcedTrack(), targetPos, true, true,
                                                            local_undo, local_redo);
    }
    timeline->requestClearSelection();
    if (!result) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        pCore->displayMessage(i18n("Cannot move selected group"), ErrorMessage);
    }
    UPDATE_UNDO_REDO_NOLOCK(local_redo, local_undo, undo, redo);
    return result;
}

bool TimelineFunctions::requestItemCopy(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int trackId, int position)
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
            res = cloneClip(timeline, id, newId, state, undo, redo);
            res = res && (newId != -1);
        }
        int target_position = timeline->getItemPosition(id) + deltaPos;
        int target_track_position = timeline->getTrackPosition(timeline->getItemTrackId(id)) + deltaTrack;
        if (target_track_position >= 0 && target_track_position < timeline->getTracksCount()) {
            auto it = timeline->m_allTracks.cbegin();
            std::advance(it, target_track_position);
            int target_track = (*it)->getId();
            if (timeline->isClip(id)) {
                res = res && timeline->requestClipMove(newId, target_track, target_position, true, true, true, true, undo, redo);
            } else {
                const QString &transitionId = timeline->m_allCompositions[id]->getAssetId();
                std::unique_ptr<Mlt::Properties> transProps(timeline->m_allCompositions[id]->properties());
                res = res && timeline->requestCompositionInsertion(transitionId, target_track, -1, target_position,
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
    qDebug() << "Successful copy, copying groups...";
    res = timeline->m_groups->copyGroups(mapping, undo, redo);
    if (!res) {
        bool undone = undo();
        Q_ASSERT(undone);
        return false;
    }
    return true;
}

void TimelineFunctions::showClipKeyframes(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, bool value)
{
    timeline->m_allClips[clipId]->setShowKeyframes(value);
    QModelIndex modelIndex = timeline->makeClipIndexFromID(clipId);
    Q_EMIT timeline->dataChanged(modelIndex, modelIndex, {TimelineModel::ShowKeyframesRole});
}

void TimelineFunctions::showCompositionKeyframes(const std::shared_ptr<TimelineItemModel> &timeline, int compoId, bool value)
{
    timeline->m_allCompositions[compoId]->setShowKeyframes(value);
    QModelIndex modelIndex = timeline->makeCompositionIndexFromID(compoId);
    Q_EMIT timeline->dataChanged(modelIndex, modelIndex, {TimelineModel::ShowKeyframesRole});
}

bool TimelineFunctions::switchEnableState(const std::shared_ptr<TimelineItemModel> &timeline, std::unordered_set<int> selection)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = false;
    bool disable = true;
    for (int clipId : selection) {
        if (!timeline->isClip(clipId)) {
            continue;
        }
        PlaylistState::ClipState oldState = timeline->getClipPtr(clipId)->clipState();
        PlaylistState::ClipState state = PlaylistState::Disabled;
        disable = true;
        if (oldState == PlaylistState::Disabled) {
            state = timeline->getTrackById_const(timeline->getClipTrackId(clipId))->trackType();
            disable = false;
        }
        result = changeClipState(timeline, clipId, state, undo, redo);
        if (!result) {
            break;
        }
    }
    // Update action name since clip will be switched
    int id = *selection.begin();
    Fun local_redo = []() { return true; };
    Fun local_undo = []() { return true; };
    if (timeline->isClip(id)) {
        bool disabled = timeline->m_allClips[id]->clipState() == PlaylistState::Disabled;
        QAction *action = pCore->window()->actionCollection()->action(QStringLiteral("clip_switch"));
        local_redo = [disabled, action]() {
            action->setText(disabled ? i18n("Enable clip") : i18n("Disable clip"));
            return true;
        };
        local_undo = [disabled, action]() {
            action->setText(disabled ? i18n("Disable clip") : i18n("Enable clip"));
            return true;
        };
    }
    if (result) {
        local_redo();
        UPDATE_UNDO_REDO_NOLOCK(local_redo, local_undo, undo, redo);
        pCore->pushUndo(undo, redo, disable ? i18n("Disable clip") : i18n("Enable clip"));
    }
    return result;
}

bool TimelineFunctions::changeClipState(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, PlaylistState::ClipState status, Fun &undo, Fun &redo)
{
    int track = timeline->getClipTrackId(clipId);
    int start = -1;
    bool invalidate = false;
    if (track > -1) {
        if (!timeline->getTrackById_const(track)->isAudioTrack()) {
            invalidate = true;
        }
        start = timeline->getItemPosition(clipId);
    }
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    // For the state change to work, we need to unplant/replant the clip
    bool result = true;
    if (track > -1) {
        result = timeline->getTrackById(track)->requestClipDeletion(clipId, true, invalidate, local_undo, local_redo, false, false);
    }
    result = timeline->m_allClips[clipId]->setClipState(status, local_undo, local_redo);
    if (result && track > -1) {
        result = timeline->getTrackById(track)->requestClipInsertion(clipId, start, true, true, local_undo, local_redo, false, false);
    }
    UPDATE_UNDO_REDO_NOLOCK(local_redo, local_undo, undo, redo);
    return result;
}

bool TimelineFunctions::requestSplitAudio(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int audioTarget)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    const std::unordered_set<int> clips = timeline->getGroupElements(clipId);
    bool done = false;
    // Now clear selection so we don't mess with groups
    timeline->requestClearSelection(false, undo, redo);
    for (int cid : clips) {
        if (!timeline->getClipPtr(cid)->canBeAudio() || timeline->getClipPtr(cid)->clipState() == PlaylistState::AudioOnly) {
            // clip without audio or audio only, skip
            pCore->displayMessage(i18n("One or more clips do not have audio, or are already audio"), ErrorMessage);
            return false;
        }
        int position = timeline->getClipPosition(cid);
        int track = timeline->getClipTrackId(cid);
        QList<int> possibleTracks;
        // Try inserting in target track first, then mirror track
        if (audioTarget >= 0) {
            possibleTracks = {audioTarget};
        }
        int mirror = timeline->getMirrorAudioTrackId(track);
        if (mirror > -1) {
            possibleTracks << mirror;
        }
        if (possibleTracks.isEmpty()) {
            // No available audio track for splitting, abort
            undo();
            pCore->displayMessage(i18n("No available audio track for restore operation"), ErrorMessage);
            return false;
        }
        int newId;
        bool res = cloneClip(timeline, cid, newId, PlaylistState::AudioOnly, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            pCore->displayMessage(i18n("Audio restore failed"), ErrorMessage);
            return false;
        }
        bool success = false;
        while (!success && !possibleTracks.isEmpty()) {
            int newTrack = possibleTracks.takeFirst();
            success = timeline->requestClipMove(newId, newTrack, position, true, true, false, true, undo, redo);
        }
        TimelineFunctions::changeClipState(timeline, cid, PlaylistState::VideoOnly, undo, redo);
        success = success && timeline->m_groups->createGroupAtSameLevel(cid, std::unordered_set<int>{newId}, GroupType::AVSplit, undo, redo);
        if (!success) {
            bool undone = undo();
            Q_ASSERT(undone);
            pCore->displayMessage(i18n("Audio restore failed"), ErrorMessage);
            return false;
        }
        done = true;
    }
    if (done) {
        timeline->requestSetSelection(clips, undo, redo);
        pCore->pushUndo(undo, redo, i18n("Restore Audio"));
    }
    return done;
}

bool TimelineFunctions::requestSplitVideo(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, int videoTarget)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    const std::unordered_set<int> clips = timeline->getGroupElements(clipId);
    bool done = false;
    // Now clear selection so we don't mess with groups
    timeline->requestClearSelection();
    for (int cid : clips) {
        if (!timeline->getClipPtr(cid)->canBeVideo() || timeline->getClipPtr(cid)->clipState() == PlaylistState::VideoOnly) {
            // clip without audio or audio only, skip
            continue;
        }
        int position = timeline->getClipPosition(cid);
        int track = timeline->getClipTrackId(cid);
        QList<int> possibleTracks;
        // Try inserting in target track first, then mirror track
        if (videoTarget >= 0) {
            possibleTracks = {videoTarget};
        }
        int mirror = timeline->getMirrorVideoTrackId(track);
        if (mirror > -1) {
            possibleTracks << mirror;
        }
        if (possibleTracks.isEmpty()) {
            // No available audio track for splitting, abort
            undo();
            pCore->displayMessage(i18n("No available video track for restore operation"), ErrorMessage);
            return false;
        }
        int newId;
        bool res = cloneClip(timeline, cid, newId, PlaylistState::VideoOnly, undo, redo);
        if (!res) {
            bool undone = undo();
            Q_ASSERT(undone);
            pCore->displayMessage(i18n("Video restore failed"), ErrorMessage);
            return false;
        }
        bool success = false;
        while (!success && !possibleTracks.isEmpty()) {
            int newTrack = possibleTracks.takeFirst();
            success = timeline->requestClipMove(newId, newTrack, position, true, true, true, true, undo, redo);
        }
        TimelineFunctions::changeClipState(timeline, cid, PlaylistState::AudioOnly, undo, redo);
        success = success && timeline->m_groups->createGroupAtSameLevel(cid, std::unordered_set<int>{newId}, GroupType::AVSplit, undo, redo);
        if (!success) {
            bool undone = undo();
            Q_ASSERT(undone);
            pCore->displayMessage(i18n("Video restore failed"), ErrorMessage);
            return false;
        }
        done = true;
    }
    if (done) {
        pCore->pushUndo(undo, redo, i18n("Restore Video"));
    }
    return done;
}

void TimelineFunctions::setCompositionATrack(const std::shared_ptr<TimelineItemModel> &timeline, int cid, int aTrack)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    std::shared_ptr<CompositionModel> compo = timeline->getCompositionPtr(cid);
    int previousATrack = compo->getATrack();
    int previousAutoTrack = static_cast<int>(compo->getForcedTrack() == -1);
    bool autoTrack = aTrack < 0;
    if (autoTrack) {
        // Automatic track compositing, find lower video track
        aTrack = timeline->getPreviousVideoTrackPos(compo->getCurrentTrackId());
    }
    int start = timeline->getItemPosition(cid);
    int end = start + timeline->getItemPlaytime(cid);
    Fun local_redo = [timeline, cid, aTrack, autoTrack, start, end]() {
        timeline->unplantComposition(cid);
        QScopedPointer<Mlt::Field> field(timeline->m_tractor->field());
        field->lock();
        timeline->getCompositionPtr(cid)->setForceTrack(!autoTrack);
        timeline->getCompositionPtr(cid)->setATrack(aTrack, aTrack < 1 ? -1 : timeline->getTrackIndexFromPosition(aTrack - 1));
        field->unlock();
        timeline->replantCompositions(cid, true);
        Q_EMIT timeline->invalidateZone(start, end);
        timeline->checkRefresh(start, end);
        return true;
    };
    Fun local_undo = [timeline, cid, previousATrack, previousAutoTrack, start, end]() {
        timeline->unplantComposition(cid);
        QScopedPointer<Mlt::Field> field(timeline->m_tractor->field());
        field->lock();
        timeline->getCompositionPtr(cid)->setForceTrack(previousAutoTrack == 0);
        timeline->getCompositionPtr(cid)->setATrack(previousATrack, previousATrack < 1 ? -1 : timeline->getTrackIndexFromPosition(previousATrack - 1));
        field->unlock();
        timeline->replantCompositions(cid, true);
        Q_EMIT timeline->invalidateZone(start, end);
        timeline->checkRefresh(start, end);
        return true;
    };
    if (local_redo()) {
        PUSH_LAMBDA(local_undo, undo);
        PUSH_LAMBDA(local_redo, redo);
    }
    pCore->pushUndo(undo, redo, i18n("Change Composition Track"));
}

QStringList TimelineFunctions::enableMultitrackView(const std::shared_ptr<TimelineItemModel> &timeline, bool enable, bool refresh)
{
    QStringList trackNames;
    std::vector<int> videoTracks;
    for (int i = 0; i < int(timeline->m_allTracks.size()); i++) {
        int tid = timeline->getTrackIndexFromPosition(i);
        if (timeline->getTrackById_const(tid)->isAudioTrack() || timeline->getTrackById_const(tid)->isHidden()) {
            continue;
        }
        videoTracks.push_back(tid);
    }
    if (videoTracks.size() < 2) {
        pCore->displayMessage(i18n("Cannot enable multitrack view on a single track"), ErrorMessage);
    }
    // First, dis/enable track compositing
    QScopedPointer<Mlt::Service> service(timeline->m_tractor->field());
    Mlt::Field *field = timeline->m_tractor->field();
    field->lock();
    while ((service != nullptr) && service->is_valid()) {
        if (service->type() == mlt_service_transition_type) {
            Mlt::Transition t(mlt_transition(service->get_service()));
            service.reset(service->producer());
            QString serviceName = t.get("mlt_service");
            int added = t.get_int("internal_added");
            if (added == 237 && serviceName != QLatin1String("mix")) {
                // Disable all compositing transitions
                t.set("disable", enable ? "1" : nullptr);
            } else if (added == 200) {
                field->disconnect_service(t);
                t.disconnect_all_producers();
            }
        } else {
            service.reset(service->producer());
        }
    }
    if (enable) {
        int count = 0;

        for (int tid : videoTracks) {
            int b_track = timeline->getTrackMltIndex(tid);
            Mlt::Transition transition(timeline->m_tractor->get_profile(), "qtblend");
            // transition.set("mlt_service", "composite");
            transition.set("a_track", 0);
            transition.set("b_track", b_track);
            // 200 is an arbitrary number so we can easily remove these transition later
            transition.set("internal_added", 200);
            QString geometry;
            trackNames << timeline->getTrackFullName(tid);
            switch (count) {
            case 0:
                switch (videoTracks.size()) {
                case 1:
                    geometry = QStringLiteral("0 0 100% 100% 100%");
                    break;
                case 2:
                    geometry = QStringLiteral("0 0 50% 100% 100%");
                    break;
                case 3:
                case 4:
                    geometry = QStringLiteral("0 0 50% 50% 100%");
                    break;
                case 5:
                case 6:
                    geometry = QStringLiteral("0 0 33% 50% 100%");
                    break;
                default:
                    geometry = QStringLiteral("0 0 33% 33% 100%");
                    break;
                }
                break;
            case 1:
                switch (videoTracks.size()) {
                case 2:
                    geometry = QStringLiteral("50% 0 50% 100% 100%");
                    break;
                case 3:
                case 4:
                    geometry = QStringLiteral("50% 0 50% 50% 100%");
                    break;
                case 5:
                case 6:
                    geometry = QStringLiteral("33% 0 33% 50% 100%");
                    break;
                default:
                    geometry = QStringLiteral("33% 0 33% 33% 100%");
                    break;
                }
                break;
            case 2:
                switch (videoTracks.size()) {
                case 3:
                case 4:
                    geometry = QStringLiteral("0 50% 50% 50% 100%");
                    break;
                case 5:
                case 6:
                    geometry = QStringLiteral("66% 0 33% 50% 100%");
                    break;
                default:
                    geometry = QStringLiteral("66% 0 33% 33% 100%");
                    break;
                }
                break;
            case 3:
                switch (videoTracks.size()) {
                case 4:
                    geometry = QStringLiteral("50% 50% 50% 50% 100%");
                    break;
                case 5:
                case 6:
                    geometry = QStringLiteral("0 50% 33% 50% 100%");
                    break;
                default:
                    geometry = QStringLiteral("0 33% 33% 33% 100%");
                    break;
                }
                break;
            case 4:
                switch (videoTracks.size()) {
                case 5:
                case 6:
                    geometry = QStringLiteral("33% 50% 33% 50% 100%");
                    break;
                default:
                    geometry = QStringLiteral("33% 33% 33% 33% 100%");
                    break;
                }
                break;
            case 5:
                switch (videoTracks.size()) {
                case 6:
                    geometry = QStringLiteral("66% 50% 33% 50% 100%");
                    break;
                default:
                    geometry = QStringLiteral("66% 33% 33% 33% 100%");
                    break;
                }
                break;
            case 6:
                geometry = QStringLiteral("0 66% 33% 33% 100%");
                break;
            case 7:
                geometry = QStringLiteral("33% 66% 33% 33% 100%");
                break;
            default:
                geometry = QStringLiteral("66% 66% 33% 33% 100%");
                break;
            }
            count++;
            // Add transition to track:
            transition.set("rect", geometry.toUtf8().constData());
            transition.set("always_active", 1);
            field->plant_transition(transition, 0, b_track);
        }
    }
    field->unlock();
    if (refresh) {
        Q_EMIT timeline->requestMonitorRefresh();
    }
    return trackNames;
}

void TimelineFunctions::saveTimelineSelection(const std::shared_ptr<TimelineItemModel> &timeline, const std::unordered_set<int> &selection,
                                              const QDir &targetDir, int duration)
{
    bool ok;
    QString name = QInputDialog::getText(qApp->activeWindow(), i18n("Add Clip to Library"), i18n("Enter a name for the clip in Library"), QLineEdit::Normal,
                                         QString(), &ok);
    if (name.isEmpty() || !ok) {
        return;
    }
    QString fullPath = targetDir.absoluteFilePath(name + QStringLiteral(".mlt"));
    if (QFile::exists(fullPath)) {
        QUrl url = QUrl::fromLocalFile(targetDir.absoluteFilePath(name + QStringLiteral(".mlt")));
        KIO::RenameDialog renameDialog(QApplication::activeWindow(), i18n("File already exists"), url, url, KIO::RenameDialog_Option::RenameDialog_Overwrite);
        if (renameDialog.exec() != QDialog::Rejected) {
            QUrl final = renameDialog.newDestUrl();
            if (final.isValid()) {
                fullPath = final.toLocalFile();
            }
        } else {
            return;
        }
    }
    int offset = -1;
    int lowerAudioTrack = -1;
    int lowerVideoTrack = -1;
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
        // Check if we have a composition with a track not yet listed
        if (timeline->isComposition(i)) {
            std::shared_ptr<CompositionModel> compo = timeline->getCompositionPtr(i);
            int aTrack = compo->getATrack();
            if (!sourceTracks.contains(aTrack)) {
                if (aTrack == 0) {
                    sourceTracks.insert(0, -1);
                } else {
                    sourceTracks.insert(aTrack, timeline->getTrackIndexFromPosition(aTrack - 1));
                }
            }
        }
    }
    qDebug() << "==========\nGOT SOUREC TRACKS: " << sourceTracks << "\n\nGGGGGGGGGGGGGGGGGGGGGGG";
    // Build target timeline
    Mlt::Tractor newTractor(pCore->getProjectProfile());
    QScopedPointer<Mlt::Field> field(newTractor.field());
    int ix = 0;
    QString composite = TransitionsRepository::get()->getCompositingTransition();
    QMapIterator<int, int> i(sourceTracks);
    QList<Mlt::Transition *> compositions;
    while (i.hasNext()) {
        i.next();
        if (i.value() == -1) {
            // Insert a black background track
            QScopedPointer<Mlt::Producer> newTrackPlaylist(new Mlt::Producer(*newTractor.profile(), "color:black"));
            newTrackPlaylist->set("kdenlive:playlistid", "black_track");
            newTrackPlaylist->set("mlt_type", "producer");
            newTrackPlaylist->set("aspect_ratio", 1);
            newTrackPlaylist->set("length", INT_MAX);
            newTrackPlaylist->set("mlt_image_format", "rgba");
            newTrackPlaylist->set("set.test_audio", 0);
            newTrackPlaylist->set_in_and_out(0, duration);
            newTractor.set_track(*newTrackPlaylist, ix);
            sourceTracks.insert(0, ix);
            ix++;
            continue;
        }
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
                    auto *t = new Mlt::Transition(*timeline->m_allCompositions[itemId].get());
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
        std::sort(compositions.begin(), compositions.end(), [](Mlt::Transition *a, Mlt::Transition *b) { return a->get_b_track() > b->get_b_track(); });
        while (!compositions.isEmpty()) {
            QScopedPointer<Mlt::Transition> t(compositions.takeFirst());
            int a_track = t->get_a_track();
            if ((sourceTracks.contains(a_track) || a_track == 0) && sourceTracks.contains(t->get_b_track())) {
                Mlt::Transition newComposition(*newTractor.profile(), t->get("mlt_service"));
                Mlt::Properties sourceProps(t->get_properties());
                newComposition.inherit(sourceProps);
                int in = qMax(0, t->get_in() - offset);
                int out = t->get_out() - offset;
                newComposition.set_in_and_out(in, out);
                if (sourceTracks.contains(a_track)) {
                    a_track = sourceTracks.value(a_track);
                }
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
        if (trackId < 0) {
            // Black background
            continue;
        }
        std::shared_ptr<TrackModel> track = timeline->getTrackById_const(trackId);
        bool isAudio = track->isAudioTrack();
        if ((isAudio && ix > lowerAudioTrack) || (!isAudio && ix > lowerVideoTrack)) {
            // add track compositing / mix
            Mlt::Transition t(*newTractor.profile(), isAudio ? "mix" : composite.toUtf8().constData());
            if (isAudio) {
                t.set("sum", 1);
                t.set("accepts_blanks", 1);
            }
            t.set("always_active", 1);
            t.set("internal_added", 237);
            t.set_tracks(isAudio ? lowerAudioTrack : lowerVideoTrack, ix);
            field->plant_transition(t, isAudio ? lowerAudioTrack : lowerVideoTrack, ix);
        }
        ix++;
    }
    QMutexLocker lock(&pCore->xmlMutex);
    Mlt::Consumer xmlConsumer(*newTractor.profile(), ("xml:" + fullPath).toUtf8().constData());
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.connect(newTractor);
    xmlConsumer.run();
}

int TimelineFunctions::getTrackOffset(const std::shared_ptr<TimelineItemModel> &timeline, int startTrack, int destTrack)
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
        if (track < 1) {
            continue;
        }
        int trackId = timeline->getTrackIndexFromPosition(track - 1);
        if (isAudio == timeline->isAudioTrack(trackId)) {
            offset += step;
        }
    }
    return offset;
}

int TimelineFunctions::getOffsetTrackId(const std::shared_ptr<TimelineItemModel> &timeline, int startTrack, int offset, bool audioOffset)
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
        if (masterTrackMltIndex < 1) {
            masterTrackMltIndex = 1;
            break;
        }
        if (masterTrackMltIndex > int(timeline->m_allTracks.size())) {
            masterTrackMltIndex = int(timeline->m_allTracks.size());
            break;
        }
        int trackId = timeline->getTrackIndexFromPosition(masterTrackMltIndex - 1);
        if (timeline->isAudioTrack(trackId) == isAudio) {
            offset += offset > 0 ? -1 : 1;
        }
    }
    return timeline->getTrackIndexFromPosition(masterTrackMltIndex - 1);
}

TimelineFunctions::TimelineTracksInfo TimelineFunctions::getAVTracksIds(const std::shared_ptr<TimelineItemModel> &timeline)
{
    TimelineTracksInfo tracks;
    for (const auto &track : timeline->m_allTracks) {
        if (track->isAudioTrack()) {
            tracks.audioIds << track->getId();
        } else {
            tracks.videoIds << track->getId();
        }
    }
    return tracks;
}

QString TimelineFunctions::copyClips(const std::shared_ptr<TimelineItemModel> &timeline, const std::unordered_set<int> &itemIds, int mainClip)
{
    int mainId = *(itemIds.begin());
    // We need to retrieve ALL the involved clips, ie those who are also grouped with the given clips
    std::unordered_set<int> allIds;
    if (timeline->singleSelectionMode()) {
        allIds = itemIds;
    } else {
        for (const auto &itemId : itemIds) {
            std::unordered_set<int> siblings = timeline->getGroupElements(itemId);
            allIds.insert(siblings.begin(), siblings.end());
        }
    }
    // Avoid using a subtitle item as reference since it doesn't work with track offset
    if (timeline->isSubTitle(mainId)) {
        for (const auto &id : allIds) {
            if (!timeline->isSubTitle(id)) {
                mainId = id;
                break;
            }
        }
    }
    bool subtitleOnlyCopy = false;
    if (timeline->isSubTitle(mainId)) {
        subtitleOnlyCopy = true;
    }

    // TODO better guess for master track
    int masterTid = timeline->getItemTrackId(mainId);
    bool audioCopy = subtitleOnlyCopy ? false : timeline->isAudioTrack(masterTid);
    int masterTrack = subtitleOnlyCopy ? -1 : timeline->getTrackPosition(masterTid);
    QDomDocument copiedItems;
    int offset = -1;
    int lastFrame = -1;
    QDomElement container = copiedItems.createElement(QStringLiteral("kdenlive-scene"));
    container.setAttribute(QStringLiteral("fps"), QString::number(pCore->getCurrentFps()));
    copiedItems.appendChild(container);
    QStringList binIds;
    for (int id : allIds) {
        int startPos = timeline->getItemPosition(id);
        if (offset == -1 || startPos < offset) {
            offset = timeline->getItemPosition(id);
        }
        if (startPos + timeline->getItemPlaytime(id) > lastFrame) {
            lastFrame = startPos + timeline->getItemPlaytime(id);
        }
        if (timeline->isClip(id)) {
            if (mainClip == -1) {
                mainClip = id;
            }
            QDomElement clipXml = timeline->m_allClips[id]->toXml(copiedItems);
            container.appendChild(clipXml);
            const QString bid = timeline->m_allClips[id]->binId();
            if (!binIds.contains(bid)) {
                binIds << bid;
            }
            int tid = timeline->getItemTrackId(id);
            if (timeline->getTrackById_const(tid)->hasStartMix(id)) {
                QDomElement mix = timeline->getTrackById_const(tid)->mixXml(copiedItems, id);
                clipXml.appendChild(mix);
            }
        } else if (timeline->isComposition(id)) {
            container.appendChild(timeline->m_allCompositions[id]->toXml(copiedItems));
        } else if (timeline->isSubTitle(id)) {
            container.appendChild(timeline->getSubtitleModel()->toXml(id, copiedItems));
        } else {
            Q_ASSERT(false);
        }
    }
    QDomElement container2 = copiedItems.createElement(QStringLiteral("bin"));
    container.appendChild(container2);
    for (const QString &id : qAsConst(binIds)) {
        std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(id);
        QDomDocument tmp;
        container2.appendChild(clip->toXml(tmp));
    }
    container.setAttribute(QStringLiteral("offset"), offset);
    container.setAttribute(QStringLiteral("duration"), lastFrame - offset);

    // Define main source for effects copy
    if (mainClip > -1) {
        if (timeline->isClip(mainClip)) {
            QStringList effectSource;
            effectSource << QString::number(mainClip);
            if (!timeline->singleSelectionMode()) {
                int partner = timeline->getClipSplitPartner(mainClip);
                if (partner > -1 && allIds.find(partner) != allIds.end()) {
                    effectSource << QString::number(partner);
                }
            }
            container.setAttribute(QStringLiteral("mainClip"), effectSource.join(QLatin1Char(';')));
        }
    }
    if (audioCopy) {
        container.setAttribute(QStringLiteral("masterAudioTrack"), masterTrack);
        int masterMirror = timeline->getMirrorVideoTrackId(masterTid);
        if (masterMirror == -1) {
            TimelineTracksInfo timelineTracks = TimelineFunctions::getAVTracksIds(timeline);
            if (!timelineTracks.videoIds.isEmpty()) {
                masterTrack = timeline->getTrackPosition(timelineTracks.videoIds.first());
            }
        } else {
            masterTrack = timeline->getTrackPosition(masterMirror);
        }
    }
    /* masterTrack contains the reference track over which we want to paste.
       this is a video track, unless audioCopy is defined */
    container.setAttribute(QStringLiteral("masterTrack"), masterTrack);
    container.setAttribute(QStringLiteral("documentid"), pCore->currentDoc()->getDocumentProperty(QStringLiteral("documentid")));
    QPair<int, int> avTracks = timeline->getAVtracksCount();
    container.setAttribute(QStringLiteral("audioTracks"), avTracks.first);
    container.setAttribute(QStringLiteral("videoTracks"), avTracks.second);
    QDomElement grp = copiedItems.createElement(QStringLiteral("groups"));
    container.appendChild(grp);
    std::unordered_set<int> groupRoots;
    std::transform(allIds.begin(), allIds.end(), std::inserter(groupRoots, groupRoots.begin()), [&](int id) {
        int parent = timeline->m_groups->getRootId(id);
        if (timeline->m_groups->getType(parent) == GroupType::Selection) {
            std::unordered_set<int> children = timeline->m_groups->getDirectChildren(parent);
            for (const auto &gid : children) {
                std::unordered_set<int> leaves = timeline->m_groups->getLeaves(gid);
                if (leaves.count(id) == 1) {
                    return gid;
                }
            }
            // This should not happen
            qDebug() << "INCORRECT GROUP ID FOUND";
            return -1;
        } else {
            return parent;
        }
    });

    qDebug() << "==============\n GROUP ROOTS: ";
    for (int gp : groupRoots) {
        qDebug() << "GROUP: " << gp;
    }
    qDebug() << "\n=======";
    grp.appendChild(copiedItems.createTextNode(timeline->m_groups->toJson(groupRoots)));

    qDebug() << " / // / PASTED DOC: \n\n" << copiedItems.toString() << "\n\n------------";
    return copiedItems.toString();
}

bool TimelineFunctions::pasteClips(const std::shared_ptr<TimelineItemModel> &timeline, const QString &pasteString, int trackId, int position)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    if (TimelineFunctions::pasteClips(timeline, pasteString, trackId, position, undo, redo)) {
        pCore->pushUndo(undo, redo, i18n("Paste clips"));
        return true;
    }
    return false;
}

bool TimelineFunctions::getUsedTracks(const QDomNodeList &clips, const QDomNodeList &compositions, int sourceMasterTrack, int &topAudioMirror, TimelineTracksInfo &allTracks, QList<int> &singleAudioTracks, std::unordered_map<int, int> &audioMirrors)
{
    // Tracks used by clips
    int max = clips.count();
    for (int i = 0; i < max; i++) {
        QDomElement clipProducer = clips.at(i).toElement();
        int trackPos = clipProducer.attribute(QStringLiteral("track")).toInt();
        if (trackPos < 0) {
            pCore->displayMessage(i18n("Not enough tracks to paste clipboard"), ErrorMessage, 500);
            semaphore.release(1);
            return false;
        }
        bool audioTrack = clipProducer.hasAttribute(QStringLiteral("audioTrack"));
        if (audioTrack) {
            if (!allTracks.audioIds.contains(trackPos)) {
                allTracks.audioIds << trackPos;
            }
            int videoMirror = clipProducer.attribute(QStringLiteral("mirrorTrack")).toInt();
            if (videoMirror == -1 || sourceMasterTrack == -1) {
                // The clip has no mirror track
                if (!singleAudioTracks.contains(trackPos)) {
                    singleAudioTracks << trackPos;
                }
                continue;
            }
            // The clip has mirror track
            audioMirrors[trackPos] = videoMirror;
            if (videoMirror > topAudioMirror) {
                // We have to check how many video tracks with mirror are needed
                topAudioMirror = videoMirror;
            }
            if (!allTracks.videoIds.contains(videoMirror)) {
                allTracks.videoIds << videoMirror;
            }
        } else {
            // Video clip
            if (!allTracks.videoIds.contains(trackPos)) {
                allTracks.videoIds << trackPos;
            }
        }
    }

    // Tracks used by compositions
    max = compositions.count();
    for (int i = 0; i < max; i++) {
        QDomElement composition = compositions.at(i).toElement();
        int trackPos = composition.attribute(QStringLiteral("track")).toInt();
        if (!allTracks.videoIds.contains(trackPos)) {
            allTracks.videoIds << trackPos;
        }
        int atrackPos = composition.attribute(QStringLiteral("a_track")).toInt();
        if (atrackPos != 0 && !allTracks.videoIds.contains(atrackPos)) {
            allTracks.videoIds << atrackPos;
        }
    }

    return true;
}

bool TimelineFunctions::pasteClipsWithUndo(const std::shared_ptr<TimelineItemModel> &timeline, const QString &pasteString, int trackId, int position, Fun &undo,
                                           Fun &redo)
{
    std::function<bool(void)> paste_undo = []() { return true; };
    std::function<bool(void)> paste_redo = []() { return true; };
    if (TimelineFunctions::pasteClips(timeline, pasteString, trackId, position, paste_undo, paste_redo)) {
        PUSH_FRONT_LAMBDA(paste_undo, undo);
        PUSH_FRONT_LAMBDA(paste_redo, redo);
        return true;
    }
    return false;
}

bool TimelineFunctions::pasteClips(const std::shared_ptr<TimelineItemModel> &timeline, const QString &pasteString, int trackId, int position, Fun &undo,
                                   Fun &redo, int inPos, int duration)
{
    timeline->requestClearSelection();
    if (!semaphore.tryAcquire(1)) {
        pCore->displayMessage(i18n("Another paste operation is in progress"), ErrorMessage, 500);
        while (!semaphore.tryAcquire(1)) {
            qApp->processEvents();
        }
    }
    waitingBinIds.clear();
    QDomDocument copiedItems;
    copiedItems.setContent(pasteString);
    if (copiedItems.documentElement().tagName() == QLatin1String("kdenlive-scene")) {
        qDebug() << " / / READING CLIPS FROM CLIPBOARD";
    } else {
        semaphore.release(1);
        pCore->displayMessage(i18n("No valid data in clipboard"), ErrorMessage, 500);
        return false;
    }
    const QString docId = copiedItems.documentElement().attribute(QStringLiteral("documentid"));
    mappedIds.clear();
    // Check available tracks
    TimelineTracksInfo timelineTracks = TimelineFunctions::getAVTracksIds(timeline);
    int sourceMasterTrack = copiedItems.documentElement().attribute(QStringLiteral("masterTrack"), QStringLiteral("-1")).toInt();
    QDomNodeList clips = copiedItems.documentElement().elementsByTagName(QStringLiteral("clip"));
    QDomNodeList compositions = copiedItems.documentElement().elementsByTagName(QStringLiteral("composition"));
    QDomNodeList subtitles = copiedItems.documentElement().elementsByTagName(QStringLiteral("subtitle"));
    // find paste tracks
    // Info about all source tracks
    TimelineTracksInfo sourceTracks;
    // List of all audio tracks with their corresponding video mirror
    std::unordered_map<int, int> audioMirrors;
    // List of all source audio tracks that don't have video mirror
    QList<int> singleAudioTracks;
    // Number of required video tracks with mirror
    int topAudioMirror = 0;

    if(!getUsedTracks(clips, compositions, sourceMasterTrack, topAudioMirror, sourceTracks, singleAudioTracks, audioMirrors)) {
        return false;
    }

    if (sourceTracks.audioIds.isEmpty() && sourceTracks.videoIds.isEmpty() && subtitles.isEmpty()) {
        // playlist does not have any tracks, exit
        semaphore.release(1);
        return true;
    }
    // Now we have a list of all source tracks, check that we have enough target tracks
    std::sort(sourceTracks.videoIds.begin(), sourceTracks.videoIds.end());
    std::sort(sourceTracks.audioIds.begin(), sourceTracks.audioIds.end());
    std::sort(singleAudioTracks.begin(), singleAudioTracks.end());

    // qDebug()<<"== GOT WANTED TKS\n VIDEO: "<<videoTracks<<"\n AUDIO TKS: "<<audioTracks<<"\n SINGLE AUDIO: "<<singleAudioTracks;
    int requestedVideoTracks = sourceTracks.videoIds.isEmpty() ? 0 : sourceTracks.videoIds.last() - sourceTracks.videoIds.first() + 1;
    int requestedAudioTracks = sourceTracks.audioIds.isEmpty() ? 0 : sourceTracks.audioIds.last() - sourceTracks.audioIds.first() + 1;
    if (requestedVideoTracks > timelineTracks.videoIds.size() || requestedAudioTracks > timelineTracks.audioIds.size()) {
        pCore->displayMessage(i18n("Not enough tracks to paste clipboard (requires %1 audio, %2 video tracks)", requestedAudioTracks, requestedVideoTracks),
                              ErrorMessage, 500);
        semaphore.release(1);
        return false;
    }

    auto findPerfectTracks = [](int &sourceTrackId, const QList<int> &sourceTracks, int &targetTrackId, const QList<int> &targetTracks) {
        const int neededTracksBelow = sourceTrackId - sourceTracks.first();
        const int neededTracksAbove = sourceTracks.last() - sourceTrackId;

        const int existingTracksBelow = targetTracks.indexOf(targetTrackId);
        const int existingTracksAbove = targetTracks.size() - (targetTracks.indexOf(targetTrackId) + 1);

        int sourceOffset = 0;
        if (neededTracksBelow < 0) {
            sourceOffset = neededTracksBelow;
        } else if (neededTracksAbove < 0) {
            sourceOffset = neededTracksAbove;
        }

        sourceTrackId += qMax(0, sourceTracks.count() - targetTracks.count() - sourceOffset);

        if (existingTracksBelow < neededTracksBelow) {
            qDebug() << "// UPDATING BELOW TID IX TO:" << neededTracksBelow;
            // not enough tracks below, try to paste on upper track
            targetTrackId = targetTracks.at(qMin(neededTracksBelow, targetTracks.length() - 1));
            return;
        }

        if (existingTracksAbove < neededTracksAbove) {
            // not enough tracks above, try to paste on lower track
            qDebug() << "// UPDATING ABOVE TID IX TO:" << (targetTracks.size() - neededTracksAbove);
            targetTrackId = targetTracks.at(qMax(0, targetTracks.size() - neededTracksAbove - 1));
            return;
        }

        // enough tracks above and below, keep the current
        // ensure it is one of the existing tracks
        targetTrackId = qBound(targetTracks.first(), targetTrackId, targetTracks.last());
    };

    // Find destination master track
    // Check we have enough tracks above/below
    if (requestedVideoTracks > 0) {
        findPerfectTracks(sourceMasterTrack, sourceTracks.videoIds, trackId, timelineTracks.videoIds);

        // Find top-most video track that requires an audio mirror
        int topAudioOffset = sourceTracks.videoIds.indexOf(topAudioMirror) - sourceTracks.videoIds.indexOf(sourceMasterTrack);
        // Check if we have enough video tracks with mirror at paste track position
        if (requestedAudioTracks > 0 && timelineTracks.audioIds.size() <= (timelineTracks.videoIds.indexOf(trackId) + topAudioOffset)) {
            int updatedPos = sourceTracks.audioIds.size() - topAudioOffset - 1;
            if (updatedPos < 0 || updatedPos >= timelineTracks.videoIds.size()) {
                pCore->displayMessage(i18n("Not enough tracks to paste clipboard"), ErrorMessage, 500);
                semaphore.release(1);
                return false;
            }
            trackId = timelineTracks.videoIds.at(updatedPos);
        }
    } else if (requestedAudioTracks > 0) {
        // Audio only
        sourceMasterTrack = copiedItems.documentElement().attribute(QStringLiteral("masterAudioTrack")).toInt();
        findPerfectTracks(sourceMasterTrack, sourceTracks.audioIds, trackId, timelineTracks.audioIds);
    }
    tracksMap.clear();
    bool audioMaster = false;
    int targetMasterIx = timelineTracks.videoIds.indexOf(trackId);
    if (targetMasterIx == -1) {
        targetMasterIx = timelineTracks.audioIds.indexOf(trackId);
        audioMaster = true;
    }

    int masterOffset = targetMasterIx - sourceMasterTrack;
    for (int tk : qAsConst(sourceTracks.videoIds)) {
        int newPos = masterOffset + tk;
        if (newPos < 0 || newPos >= timelineTracks.videoIds.size()) {
            pCore->displayMessage(i18n("Not enough tracks to paste clipboard"), ErrorMessage, 500);
            semaphore.release(1);
            return false;
        }
        tracksMap.insert(tk, timelineTracks.videoIds.at(newPos));
        // qDebug() << "/// MAPPING SOURCE TRACK: "<<tk<<" TO PROJECT TK: "<<timelineTracks.videoIds.at(newPos)<<" =
        // "<<timeline->getTrackMltIndex(timelineTracks.videoIds.at(newPos));
    }
    bool audioOffsetCalculated = false;
    int audioOffset = 0;
    for (const auto &mirror : audioMirrors) {
        int videoIx = tracksMap.value(mirror.second);
        int mirrorIx = timeline->getMirrorAudioTrackId(videoIx);
        if (mirrorIx > 0) {
            tracksMap.insert(mirror.first, mirrorIx);
            if (!audioOffsetCalculated) {
                int oldPosition = mirror.first;
                int currentPosition = timeline->getTrackPosition(tracksMap.value(oldPosition));
                audioOffset = currentPosition - oldPosition;
                audioOffsetCalculated = true;
            }
        }
    }
    if (!audioOffsetCalculated && audioMaster) {
        audioOffset = masterOffset;
        audioOffsetCalculated = true;
    } else if (audioMirrors.size() == 0) {
        // We are passing ungrouped audio clips, calculate offset
        int sourceAudioTracks = copiedItems.documentElement().attribute(QStringLiteral("audioTracks")).toInt();
        if (sourceAudioTracks > 0) {
            audioOffset = timelineTracks.audioIds.count() - sourceAudioTracks;
        }
    }
    for (int oldPos : qAsConst(singleAudioTracks)) {
        if (tracksMap.contains(oldPos)) {
            continue;
        }
        int offsetId = oldPos + audioOffset;
        if (offsetId < 0 || offsetId >= timelineTracks.audioIds.size()) {
            pCore->displayMessage(i18n("Not enough tracks to paste clipboard"), ErrorMessage, 500);
            semaphore.release(1);
            return false;
        }
        tracksMap.insert(oldPos, timelineTracks.audioIds.at(offsetId));
    }
    std::function<void(const QString &)> callBack = [timeline, copiedItems, position, inPos, duration](const QString &binId) {
        waitingBinIds.removeAll(binId);
        if (waitingBinIds.isEmpty()) {
            TimelineFunctions::pasteTimelineClips(timeline, copiedItems, position, inPos, duration);
        }
    };
    bool clipsImported = false;
    int updatedPosition = 0;
    int pasteDuration = copiedItems.documentElement().attribute(QStringLiteral("duration")).toInt();
    if (docId == pCore->currentDoc()->getDocumentProperty(QStringLiteral("documentid"))) {
        // Check that the bin clips exists in case we try to paste in a copy of original project
        QDomNodeList binClips = copiedItems.documentElement().elementsByTagName(QStringLiteral("producer"));
        QString folderId = pCore->projectItemModel()->getFolderIdByName(i18n("Pasted clips"));
        for (int i = 0; i < binClips.count(); ++i) {
            QDomElement currentProd = binClips.item(i).toElement();
            QString clipId = Xml::getXmlProperty(currentProd, QStringLiteral("kdenlive:id"));
            if (clipId.isEmpty()) {
                // Invalid clip, maybe black track from a sequence, ignore
                continue;
            }
            QString clipHash = Xml::getXmlProperty(currentProd, QStringLiteral("kdenlive:file_hash"));
            if (!pCore->projectItemModel()->validateClip(clipId, clipHash)) {
                // This clip is different in project and in paste data, create a copy
                QString updatedId = QString::number(pCore->projectItemModel()->getFreeClipId());
                Xml::setXmlProperty(currentProd, QStringLiteral("kdenlive:id"), updatedId);
                mappedIds.insert(clipId, updatedId);
                if (folderId.isEmpty()) {
                    // Folder does not exist
                    const QString rootId = pCore->projectItemModel()->getRootFolder()->clipId();
                    folderId = QString::number(pCore->projectItemModel()->getFreeFolderId());
                    pCore->projectItemModel()->requestAddFolder(folderId, i18n("Pasted clips"), rootId, undo, redo);
                }
                waitingBinIds << updatedId;
                clipsImported = true;
                pCore->projectItemModel()->requestAddBinClip(updatedId, currentProd, folderId, undo, redo, callBack);
            }
        }
        updatedPosition = position + pasteDuration;
    }

    if (!docId.isEmpty() && docId != pCore->currentDoc()->getDocumentProperty(QStringLiteral("documentid"))) {
        // paste from another document, import bin clips

        // Check if the fps matches
        QString currentFps = QString::number(pCore->getCurrentFps());
        QString sourceFps = copiedItems.documentElement().attribute(QStringLiteral("fps"));
        double ratio = 1.;
        if (currentFps != sourceFps && !sourceFps.isEmpty()) {
            if (KMessageBox::questionTwoActions(
                    pCore->window(),
                    i18n("The source project has a different framerate (%1fps) than your current project.<br/>Clips or keyframes might be messed up.",
                         sourceFps),
                    i18n("Pasting Warning"), KGuiItem(i18n("Paste")), KStandardGuiItem::cancel()) != KMessageBox::PrimaryAction) {
                semaphore.release(1);
                return false;
            }
            ratio = pCore->getCurrentFps() / sourceFps.toDouble();
            copiedItems.documentElement().setAttribute(QStringLiteral("fps-ratio"), ratio);
        }

        // Folder in the project for the pasted clips
        QString folderId = pCore->projectItemModel()->getFolderIdByName(i18n("Pasted clips"));
        if (folderId.isEmpty()) {
            // Folder does not exist
            const QString rootId = pCore->projectItemModel()->getRootFolder()->clipId();
            folderId = QString::number(pCore->projectItemModel()->getFreeFolderId());
            pCore->projectItemModel()->requestAddFolder(folderId, i18n("Pasted clips"), rootId, undo, redo);
        }
        updatedPosition = position + (pasteDuration * ratio);

        auto disableProxy = [](QDomElement &producer) {
            const QString proxy = Xml::getXmlProperty(producer, QStringLiteral("kdenlive:proxy"));
            if (proxy.length() < 4) {
                return;
            }
            const QString resource = Xml::getXmlProperty(producer, QStringLiteral("kdenlive:originalurl"));
            if (!resource.isEmpty()) {
                Xml::setXmlProperty(producer, QStringLiteral("resource"), resource);
                Xml::setXmlProperty(producer, QStringLiteral("kdenlive:proxy"), QStringLiteral("-"));
            }
        };

        auto useFreeBinId = [](QDomElement &producer, const QString &clipId, QMap<QString, QString> &mappedIds) {
            if (!pCore->projectItemModel()->isIdFree(clipId)) {
                QString updatedId = QString::number(pCore->projectItemModel()->getFreeClipId());
                Xml::setXmlProperty(producer, QStringLiteral("kdenlive:id"), updatedId);
                mappedIds.insert(clipId, updatedId);
                return updatedId;
            }
            return clipId;
        };

        auto pasteClip = [disableProxy, callBack, useFreeBinId](const QDomNodeList &clips, int ratio, const QString &folderId, bool &clipsImported, Fun &undo,
                                                                Fun &redo){
            for (int i = 0; i < clips.count(); ++i) {
                QDomElement currentProd = clips.item(i).toElement();
                QString clipId = Xml::getXmlProperty(currentProd, QStringLiteral("kdenlive:id"));
                if (clipId.isEmpty()) {
                    // Not a bin clip
                    continue;
                }

                // Adjust duration in case of different fps on source and target
                if (ratio != 1.) {
                    int out = currentProd.attribute(QStringLiteral("out")).toInt() * ratio;
                    int length = Xml::getXmlProperty(currentProd, QStringLiteral("length")).toInt() * ratio;
                    currentProd.setAttribute(QStringLiteral("out"), out);
                    Xml::setXmlProperty(currentProd, QStringLiteral("length"), QString::number(length));
                }

                // Check if we already have a clip with same hash in pasted clips folder
                QString clipHash = Xml::getXmlProperty(currentProd, QStringLiteral("kdenlive:file_hash"));
                QString existingId = pCore->projectItemModel()->validateClipInFolder(folderId, clipHash);
                if (!existingId.isEmpty()) {
                    mappedIds.insert(clipId, existingId);
                    continue;
                }
                clipId = useFreeBinId(currentProd, clipId, mappedIds);

                // Disable proxy if any when pasting to another document
                disableProxy(currentProd);

                waitingBinIds << clipId;
                clipsImported = true;
                bool insert = pCore->projectItemModel()->requestAddBinClip(clipId, currentProd, folderId, undo, redo, callBack);
                if (!insert) {
                    return false;
                }
            }
            return true;
        };

        QDomNodeList binClips = copiedItems.documentElement().elementsByTagName(QStringLiteral("producer"));
        if (!pasteClip(binClips, ratio, folderId, clipsImported, undo, redo)) {
            pCore->displayMessage(i18n("Could not add bin clip"), ErrorMessage, 500);
            undo();
            semaphore.release(1);
            return false;
        }

        QDomNodeList chainClips = copiedItems.documentElement().elementsByTagName(QStringLiteral("chain"));
        if (!pasteClip(chainClips, ratio, folderId, clipsImported, undo, redo)) {
            pCore->displayMessage(i18n("Could not add bin clip"), ErrorMessage, 500);
            undo();
            semaphore.release(1);
            return false;
        }

        auto remapClipIds = [](QDomNodeList &elements, const QMap<QString, QString> &map) {
            int max = elements.count();
            for (int i = 0; i < max; i++) {
                QDomElement e = elements.item(i).toElement();
                const QString currentId = Xml::getXmlProperty(e, QStringLiteral("kdenlive:id"));
                if (map.contains(currentId)) {
                    Xml::setXmlProperty(e, QStringLiteral("kdenlive:id"), map.value(currentId));
                }
            }
        };

        QDomNodeList sequenceClips = copiedItems.documentElement().elementsByTagName(QStringLiteral("mlt"));
        for (int i = 0; i < sequenceClips.count(); ++i) {
            QDomElement currentProd = sequenceClips.item(i).toElement();
            QString clipId = currentProd.attribute(QStringLiteral("kdenlive:id"));
            const QString uuid = currentProd.attribute(QStringLiteral("kdenlive:uuid"));
            int duration = currentProd.attribute(QStringLiteral("kdenlive:duration")).toInt();
            const QString clipname = currentProd.attribute(QStringLiteral("kdenlive:clipname"));
            if (clipId.isEmpty()) {
                // Not a bin clip
                continue;
            }

            QDomDocument doc;
            doc.appendChild(doc.importNode(currentProd, true));
            clipId = useFreeBinId(currentProd, clipId, mappedIds);

            // update all bin ids
            QDomNodeList prods = doc.documentElement().elementsByTagName(QStringLiteral("producer"));
            remapClipIds(prods, mappedIds);
            QDomNodeList chains = doc.documentElement().elementsByTagName(QStringLiteral("chain"));
            remapClipIds(chains, mappedIds);
            QDomNodeList entries = doc.documentElement().elementsByTagName(QStringLiteral("entry"));
            remapClipIds(entries, mappedIds);

            waitingBinIds << clipId;
            clipsImported = true;
            QMutexLocker lock(&pCore->xmlMutex);
            std::shared_ptr<Mlt::Producer> xmlProd(new Mlt::Producer(pCore->getProjectProfile(), "xml-string", doc.toString().toUtf8().constData()));
            lock.unlock();
            if (!xmlProd->is_valid()) {
                qDebug() << ":::: CANNOT IMPORT SEQUENCE: " << clipId;
                continue;
            }
            xmlProd->set("kdenlive:id", clipId.toUtf8().constData());
            xmlProd->set("kdenlive:producer_type", ClipType::Timeline);
            xmlProd->set("kdenlive:uuid", uuid.toUtf8().constData());
            xmlProd->set("kdenlive:duration", xmlProd->frames_to_time(duration));
            xmlProd->set("kdenlive:clipname", clipname.toUtf8().constData());
            xmlProd->set("_kdenlive_processed", 1);
            Mlt::Service s(*xmlProd.get());
            Mlt::Tractor tractor(s);
            std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(tractor.cut()));
            prod->set("id", uuid.toUtf8().constData());
            prod->set("kdenlive:id", clipId.toUtf8().constData());
            prod->set("kdenlive:producer_type", ClipType::Timeline);
            prod->set("kdenlive:uuid", uuid.toUtf8().constData());
            prod->set("kdenlive:duration", xmlProd->frames_to_time(duration));
            prod->set("kdenlive:clipname", clipname.toUtf8().constData());
            prod->set("_kdenlive_processed", 1);
            bool insert = pCore->projectItemModel()->requestAddBinClip(clipId, prod, folderId, undo, redo, callBack);
            if (!insert) {
                pCore->displayMessage(i18n("Could not add bin clip"), ErrorMessage, 500);
                undo();
                semaphore.release(1);
                return false;
            }
        }
    }

    if (!clipsImported) {
        // Clips from same document, directly proceed to pasting
        bool result = TimelineFunctions::pasteTimelineClips(timeline, copiedItems, position, undo, redo, false, inPos, duration);
        if (result && updatedPosition > 0) {
            pCore->seekMonitor(Kdenlive::ProjectMonitor, updatedPosition);
        }
        return result;
    }
    qDebug() << "++++++++++++\nWAITIND FOR BIN INSERTION: " << waitingBinIds << "\n\n+++++++++++++";
    return true;
}

bool TimelineFunctions::pasteTimelineClips(const std::shared_ptr<TimelineItemModel> &timeline, const QDomDocument &copiedItems, int position, int inPos,
                                           int duration)
{
    std::function<bool(void)> timeline_undo = []() { return true; };
    std::function<bool(void)> timeline_redo = []() { return true; };
    return TimelineFunctions::pasteTimelineClips(timeline, copiedItems, position, timeline_undo, timeline_redo, true, inPos, duration);
}

bool TimelineFunctions::pasteTimelineClips(const std::shared_ptr<TimelineItemModel> &timeline, QDomDocument copiedItems, int position, Fun &timeline_undo,
                                           Fun &timeline_redo, bool pushToStack, int inPos, int duration)
{
    // Wait until all bin clips are inserted
    QDomNodeList clips = copiedItems.documentElement().elementsByTagName(QStringLiteral("clip"));
    QDomNodeList compositions = copiedItems.documentElement().elementsByTagName(QStringLiteral("composition"));
    QDomNodeList subtitles = copiedItems.documentElement().elementsByTagName(QStringLiteral("subtitle"));
    int offset = copiedItems.documentElement().attribute(QStringLiteral("offset")).toInt();
    bool res = true;
    std::unordered_map<int, int> correspondingIds;
    double ratio = 1.0;
    if (copiedItems.documentElement().hasAttribute(QStringLiteral("fps-ratio"))) {
        ratio = copiedItems.documentElement().attribute(QStringLiteral("fps-ratio")).toDouble();
        offset *= ratio;
    }

    QDomElement documentMixes = copiedItems.createElement(QStringLiteral("mixes"));
    for (int i = 0; i < clips.count(); i++) {
        QDomElement prod = clips.at(i).toElement();
        QString originalId = prod.attribute(QStringLiteral("binid"));
        if (mappedIds.contains(originalId)) {
            // Map id
            originalId = mappedIds.value(originalId);
        }
        if (!pCore->projectItemModel()->hasClip(originalId)) {
            // Clip import was not successful, continue
            pCore->displayMessage(i18n("All clips were not successfully copied"), ErrorMessage, 500);
            continue;
        }
        int in = prod.attribute(QStringLiteral("in")).toInt();
        int out = prod.attribute(QStringLiteral("out")).toInt();
        int curTrackId = tracksMap.value(prod.attribute(QStringLiteral("track")).toInt());
        if (!timeline->isTrack(curTrackId)) {
            // Something is broken
            pCore->displayMessage(i18n("Not enough tracks to paste clipboard"), ErrorMessage, 500);
            timeline_undo();
            semaphore.release(1);
            return false;
        }
        int pos = prod.attribute(QStringLiteral("position")).toInt();
        if (ratio != 1.0) {
            in = in * ratio;
            out = out * ratio;
            pos = pos * ratio;
        }
        int newIn = in;
        int newOut = out;
        if ((inPos > 0 && pos + (out - in) < inPos + offset) || (duration > -1 && (pos > inPos + duration + offset))) {
            // Clip outside paste range
            continue;
        }
        if (inPos > 0) {
            pos -= inPos;
            if (pos < offset) {
                newIn = in + (offset - pos);
                pos = offset;
            }
        }
        if (duration > -1) {
            if (pos + (out - in) > inPos + duration + offset) {
                newOut = out - (pos + (out - in) - (inPos + duration + offset));
            }
        }

        pos -= offset;
        double speed = prod.attribute(QStringLiteral("speed")).toDouble();
        bool warp_pitch = false;
        if (!qFuzzyCompare(speed, 1.)) {
            warp_pitch = prod.attribute(QStringLiteral("warp_pitch")).toInt();
        }
        int audioStream = prod.attribute(QStringLiteral("audioStream")).toInt();
        int newId;
        bool created = timeline->requestClipCreation(originalId, newId, timeline->getTrackById_const(curTrackId)->trackType(), audioStream, speed, warp_pitch,
                                                     timeline_undo, timeline_redo);
        if (!created) {
            // Something is broken
            pCore->displayMessage(i18n("Could not paste items in timeline"), ErrorMessage, 500);
            timeline_undo();
            semaphore.release(1);
            return false;
        }
        if (prod.hasAttribute(QStringLiteral("timemap"))) {
            // This is a timeremap
            timeline->m_allClips[newId]->useTimeRemapProducer(true, timeline_undo, timeline_redo);
            if (timeline->m_allClips[newId]->m_producer->parent().type() == mlt_service_chain_type) {
                Mlt::Chain fromChain(timeline->m_allClips[newId]->m_producer->parent());
                int count = fromChain.link_count();
                for (int i = 0; i < count; i++) {
                    QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
                    if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                        if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                            // Found a timeremap effect, read params
                            fromLink->set("time_map", prod.attribute(QStringLiteral("timemap")).toUtf8().constData());
                            fromLink->set("pitch", prod.attribute(QStringLiteral("timepitch")).toInt());
                            fromLink->set("image_mode", prod.attribute(QStringLiteral("timeblend")).toUtf8().constData());
                            break;
                        }
                    }
                }
            }
        }
        if (timeline->m_allClips[newId]->m_endlessResize) {
            out = out - in;
            in = 0;
            timeline->m_allClips[newId]->m_producer->set("length", out + 1);
            timeline->m_allClips[newId]->m_producer->set("out", out);
        }
        timeline->m_allClips[newId]->setInOut(in, out);
        int targetId = prod.attribute(QStringLiteral("id")).toInt();
        int targetPlaylist = prod.attribute(QStringLiteral("playlist")).toInt();
        if (targetPlaylist > 0) {
            timeline->m_allClips[newId]->setSubPlaylistIndex(targetPlaylist, curTrackId);
        }
        correspondingIds[targetId] = newId;
        std::shared_ptr<EffectStackModel> destStack = timeline->getClipEffectStackModel(newId);
        destStack->fromXml(prod.firstChildElement(QStringLiteral("effects")), timeline_undo, timeline_redo);
        if (newIn != in) {
            int newSize = out - newIn + 1;
            res = res && timeline->requestItemResize(newId, newSize, false, true, timeline_undo, timeline_redo);
            if (res) {
                std::shared_ptr<EffectStackModel> sourceStack = timeline->getClipEffectStackModel(newId);
                sourceStack->cleanFadeEffects(true, timeline_undo, timeline_redo);
            }
            // TODO manage mixes
        }
        if (newOut != out) {
            int newSize = newOut - newIn;
            res = res && timeline->requestItemResize(newId, newSize, true, true, timeline_undo, timeline_redo);
            if (res) {
                std::shared_ptr<EffectStackModel> sourceStack = timeline->getClipEffectStackModel(newId);
                sourceStack->cleanFadeEffects(false, timeline_undo, timeline_redo);
            }
            // TODO manage mixes
        }
        res = res && timeline->getTrackById(curTrackId)->requestClipInsertion(newId, position + pos, true, true, timeline_undo, timeline_redo);
        // paste effects
        if (!res) {
            qDebug() << "=== COULD NOT PASTE CLIP: " << newId << " ON TRACK: " << curTrackId << " AT: " << position;
            break;
        }
        // Mixes (same track transitions)
        if (prod.hasChildNodes()) {
            // TODO: adjust position/duration with inPos / duration
            QDomNodeList mixes = prod.elementsByTagName(QLatin1String("mix"));
            if (!mixes.isEmpty()) {
                QDomElement mix = mixes.at(0).toElement();
                if (mix.tagName() == QLatin1String("mix")) {
                    mix.setAttribute(QStringLiteral("tid"), curTrackId);
                    documentMixes.appendChild(mix);
                }
            }
        }
    }
    // Process mix insertion
    QDomNodeList mixes = documentMixes.childNodes();
    for (int k = 0; k < mixes.count(); k++) {
        QDomElement mix = mixes.at(k).toElement();
        int originalFirstClipId = mix.attribute(QLatin1String("firstClip")).toInt();
        int originalSecondClipId = mix.attribute(QLatin1String("secondClip")).toInt();
        if (correspondingIds.count(originalFirstClipId) > 0 && correspondingIds.count(originalSecondClipId) > 0) {
            QVector<QPair<QString, QVariant>> params;
            QDomNodeList paramsXml = mix.elementsByTagName(QLatin1String("param"));
            for (int j = 0; j < paramsXml.count(); j++) {
                QDomElement e = paramsXml.at(j).toElement();
                params.append({e.attribute(QLatin1String("name")), e.text()});
            }
            std::pair<QString, QVector<QPair<QString, QVariant>>> mixParams = {mix.attribute(QLatin1String("asset")), params};
            MixInfo mixData;
            mixData.firstClipId = correspondingIds[originalFirstClipId];
            mixData.secondClipId = correspondingIds[originalSecondClipId];
            mixData.firstClipInOut.second = mix.attribute(QLatin1String("mixEnd")).toInt() * ratio;
            mixData.secondClipInOut.first = mix.attribute(QLatin1String("mixStart")).toInt() * ratio;
            mixData.mixOffset = mix.attribute(QLatin1String("mixOffset")).toInt() * ratio;
            std::pair<int, int> tracks = {mix.attribute(QLatin1String("a_track")).toInt(), mix.attribute(QLatin1String("b_track")).toInt()};
            if (tracks.first == tracks.second) {
                tracks = {0, 1};
            }
            timeline->getTrackById_const(mix.attribute(QLatin1String("tid")).toInt())->createMix(mixData, mixParams, tracks, true);
        }
    }
    // Compositions
    if (res) {
        for (int i = 0; res && i < compositions.count(); i++) {
            QDomElement prod = compositions.at(i).toElement();
            QString originalId = prod.attribute(QStringLiteral("composition"));
            int in = prod.attribute(QStringLiteral("in")).toInt() * ratio;
            int out = prod.attribute(QStringLiteral("out")).toInt() * ratio;
            int pos = prod.attribute(QStringLiteral("position")).toInt() * ratio - offset;
            int newPos = pos;
            if (inPos > 0) {
                newPos -= inPos;
            }
            int compoDuration = out - in + 1;
            int compoDuration2 = out - in + 1;
            if (newPos < 0) {
                // resize composition
                compoDuration += newPos;
                newPos = 0;
            }
            if (duration > -1 && (newPos + compoDuration > inPos + duration)) {
                compoDuration2 = inPos + duration - newPos;
            }
            if (compoDuration2 <= 0) {
                continue;
            }
            int curTrackId = tracksMap.value(prod.attribute(QStringLiteral("track")).toInt());
            int trackOffset = Xml::getXmlProperty(prod, QStringLiteral("b_track")).toInt() - Xml::getXmlProperty(prod, QStringLiteral("a_track")).toInt();
            // Add 1 to account for the black track
            int aTrackPos = timeline->getTrackPosition(curTrackId) - trackOffset + 1;
            int atrackId = -1;
            if (aTrackPos > 0 && aTrackPos < timeline->getTracksCount()) {
                atrackId = timeline->getTrackIndexFromPosition(aTrackPos - 1);
            }
            if (atrackId > -1 && !timeline->isAudioTrack(atrackId)) {
                // Ok, track found
            } else {
                aTrackPos = 0;
            }

            int newId;
            auto transProps = std::make_unique<Mlt::Properties>();
            QDomNodeList props = prod.elementsByTagName(QStringLiteral("property"));
            for (int j = 0; j < props.count(); j++) {
                transProps->set(props.at(j).toElement().attribute(QStringLiteral("name")).toUtf8().constData(),
                                props.at(j).toElement().text().toUtf8().constData());
            }
            res = res && timeline->requestCompositionCreation(originalId, out - in + 1, std::move(transProps), newId, timeline_undo, timeline_redo);
            if (newPos != pos) {
                // transition start resized
                timeline->requestItemResize(newId, compoDuration, false, true, timeline_undo, timeline_redo, false);
            }
            if (compoDuration != compoDuration2) {
                timeline->requestItemResize(newId, compoDuration2, true, true, timeline_undo, timeline_redo, false);
            }
            res = res && timeline->requestCompositionMove(newId, curTrackId, aTrackPos, position + newPos, true, true, timeline_undo, timeline_redo);
        }
    }
    if (res && !subtitles.isEmpty()) {
        auto subModel = timeline->getSubtitleModel();
        if (!subModel) {
            // This timeline doesn't yet have subtitles, initiate
            pCore->window()->slotShowSubtitles(true);
            subModel = timeline->getSubtitleModel();
        }
        for (int i = 0; res && i < subtitles.count(); i++) {
            QDomElement prod = subtitles.at(i).toElement();
            int in = prod.attribute(QStringLiteral("in")).toInt() * ratio - offset;
            int out = prod.attribute(QStringLiteral("out")).toInt() * ratio - offset;
            QString text = prod.attribute(QStringLiteral("text"));
            res = res && subModel->addSubtitle(GenTime(position + in, pCore->getCurrentFps()), GenTime(position + out, pCore->getCurrentFps()), text,
                                               timeline_undo, timeline_redo);
        }
    }
    if (!res) {
        timeline_undo();
        pCore->displayMessage(i18n("Could not paste items in timeline"), ErrorMessage, 500);
        semaphore.release(1);
        return false;
    }
    // Rebuild groups
    const QString groupsData = copiedItems.documentElement().firstChildElement(QStringLiteral("groups")).text();
    if (!groupsData.isEmpty()) {
        timeline->m_groups->fromJsonWithOffset(groupsData, tracksMap, position - offset, ratio, timeline_undo, timeline_redo);
    }
    // Ensure to clear selection in undo/redo too.
    Fun unselect = [timeline]() {
        qDebug() << "starting undo or redo. Selection " << timeline->m_currentSelection.size();
        timeline->requestClearSelection();
        qDebug() << "after Selection " << timeline->m_currentSelection.size();
        return true;
    };
    PUSH_FRONT_LAMBDA(unselect, timeline_undo);
    PUSH_FRONT_LAMBDA(unselect, timeline_redo);
    // UPDATE_UNDO_REDO_NOLOCK(timeline_redo, timeline_undo, undo, redo);
    if (pushToStack) {
        pCore->pushUndo(timeline_undo, timeline_redo, i18n("Paste timeline clips"));
    }
    semaphore.release(1);
    return true;
}

bool TimelineFunctions::requestDeleteBlankAt(const std::shared_ptr<TimelineItemModel> &timeline, int trackId, int position, bool affectAllTracks)
{
    // Check we have blank at position
    int startPos = -1;
    int endPos = -1;
    if (affectAllTracks) {
        for (const auto &track : timeline->m_allTracks) {
            if (!track->isLocked()) {
                if (!track->isBlankAt(position)) {
                    return false;
                }
                startPos = track->getBlankStart(position) - 1;
                endPos = track->getBlankEnd(position) + 2;
                if (startPos > -1) {
                    std::unordered_set<int> clips = timeline->getItemsInRange(trackId, startPos, endPos);
                    if (clips.size() == 2) {
                        auto it = clips.begin();
                        int firstCid = *it;
                        ++it;
                        int lastCid = *it;
                        if (timeline->m_groups->isInGroup(firstCid)) {
                            int groupId = timeline->m_groups->getRootId(firstCid);
                            std::unordered_set<int> all_children = timeline->m_groups->getLeaves(groupId);
                            if (all_children.find(lastCid) != all_children.end()) {
                                return false;
                            }
                        }
                    }
                }
            }
        }
        // check subtitle track
        if (timeline->getSubtitleModel() && !timeline->getSubtitleModel()->isLocked()) {
            if (!timeline->getSubtitleModel()->isBlankAt(position)) {
                return false;
            }
            startPos = timeline->getSubtitleModel()->getBlankStart(position) - 1;
            endPos = timeline->getSubtitleModel()->getBlankEnd(position) + 1;
            if (startPos > -1) {
                std::unordered_set<int> clips = timeline->getItemsInRange(trackId, startPos, endPos);
                if (clips.size() == 2) {
                    auto it = clips.begin();
                    int firstCid = *it;
                    ++it;
                    int lastCid = *it;
                    if (timeline->m_groups->isInGroup(firstCid)) {
                        int groupId = timeline->m_groups->getRootId(firstCid);
                        std::unordered_set<int> all_children = timeline->m_groups->getLeaves(groupId);
                        if (all_children.find(lastCid) != all_children.end()) {
                            return false;
                        }
                    }
                }
            }
        }
    } else {
        // Check we have a blank and that it is in not between 2 grouped clips
        if (timeline->trackIsLocked(trackId)) {
            timeline->flashLock(trackId);
            return false;
        }
        if (timeline->isSubtitleTrack(trackId)) {
            // Subtitle track
            if (!timeline->getSubtitleModel()->isBlankAt(position)) {
                return false;
            }
            startPos = timeline->getSubtitleModel()->getBlankStart(position) - 1;
            endPos = timeline->getSubtitleModel()->getBlankEnd(position) + 1;
        } else {
            if (!timeline->getTrackById_const(trackId)->isBlankAt(position)) {
                return false;
            }
            startPos = timeline->getTrackById_const(trackId)->getBlankStart(position) - 1;
            endPos = timeline->getTrackById_const(trackId)->getBlankEnd(position) + 2;
        }
        if (startPos > -1) {
            std::unordered_set<int> clips = timeline->getItemsInRange(trackId, startPos, endPos);
            if (clips.size() == 2) {
                auto it = clips.begin();
                int firstCid = *it;
                ++it;
                int lastCid = *it;
                if (timeline->m_groups->isInGroup(firstCid)) {
                    int groupId = timeline->m_groups->getRootId(firstCid);
                    std::unordered_set<int> all_children = timeline->m_groups->getLeaves(groupId);
                    if (all_children.find(lastCid) != all_children.end()) {
                        return false;
                    }
                }
            }
        }
    }
    std::pair<int, int> spacerOp = requestSpacerStartOperation(timeline, affectAllTracks ? -1 : trackId, position);
    int cid = spacerOp.first;
    if (cid == -1 || spacerOp.second == -1) {
        return false;
    }
    int start = timeline->getItemPosition(cid);
    int spaceStart = start - spacerOp.second;
    if (spaceStart >= start) {
        return false;
    }
    // Start undoable command
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    requestSpacerEndOperation(timeline, cid, start, spaceStart, affectAllTracks ? -1 : trackId, KdenliveSettings::lockedGuides() ? -1 : position, undo, redo);
    return true;
}

bool TimelineFunctions::requestDeleteAllBlanksFrom(const std::shared_ptr<TimelineItemModel> &timeline, int trackId, int position)
{
    // Abort if track is locked
    if (timeline->trackIsLocked(trackId)) {
        timeline->flashLock(trackId);
        return false;
    }
    // Start undoable command
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    if (timeline->isSubtitleTrack(trackId)) {
        // Subtitle track
        int blankStart = timeline->getSubtitleModel()->getNextBlankStart(position);
        if (blankStart == -1) {
            return false;
        }
        while (blankStart != -1) {
            std::pair<int, int> spacerOp = requestSpacerStartOperation(timeline, trackId, blankStart, true);
            int cid = spacerOp.first;
            if (cid == -1) {
                break;
            }
            int start = timeline->getItemPosition(cid);
            // Start undoable command
            std::function<bool(void)> local_undo = []() { return true; };
            std::function<bool(void)> local_redo = []() { return true; };
            if (blankStart < start) {
                if (!requestSpacerEndOperation(timeline, cid, start, blankStart, trackId, KdenliveSettings::lockedGuides() ? -1 : position, local_undo,
                                               local_redo, false)) {
                    // Failed to remove blank, maybe blocked because of a group. Pass to the next one
                    blankStart = start;
                } else {
                    UPDATE_UNDO_REDO_NOLOCK(local_redo, local_undo, undo, redo);
                }
            } else {
                if (timeline->getSubtitleModel()->isBlankAt(blankStart)) {
                    blankStart = timeline->getSubtitleModel()->getBlankEnd(blankStart) + 1;
                    if (blankStart == 1) {
                        break;
                    }
                } else {
                    blankStart = start + timeline->getItemPlaytime(cid) + 1;
                }
            }
            int nextBlank = timeline->getSubtitleModel()->getNextBlankStart(blankStart);
            if (nextBlank == blankStart) {
                blankStart = timeline->getSubtitleModel()->getBlankEnd(blankStart) + 1;
                nextBlank = timeline->getSubtitleModel()->getNextBlankStart(blankStart);
                if (nextBlank == blankStart) {
                    break;
                }
            }
            if (nextBlank < blankStart) {
                // Done
                break;
            }
            blankStart = nextBlank;
        }
    } else {
        int blankStart = timeline->getTrackById_const(trackId)->getNextBlankStart(position);
        if (blankStart == -1) {
            return false;
        }
        while (blankStart != -1) {
            std::pair<int, int> spacerOp = requestSpacerStartOperation(timeline, trackId, blankStart, true);
            int cid = spacerOp.first;
            if (cid == -1) {
                break;
            }
            int start = timeline->getItemPosition(cid);
            // Start undoable command
            std::function<bool(void)> local_undo = []() { return true; };
            std::function<bool(void)> local_redo = []() { return true; };
            if (blankStart < start) {
                if (!requestSpacerEndOperation(timeline, cid, start, blankStart, trackId, KdenliveSettings::lockedGuides() ? -1 : position, local_undo,
                                               local_redo, false)) {
                    // Failed to remove blank, maybe blocked because of a group. Pass to the next one
                    blankStart = start;
                } else {
                    UPDATE_UNDO_REDO_NOLOCK(local_redo, local_undo, undo, redo);
                }
            } else {
                if (timeline->getTrackById_const(trackId)->isBlankAt(blankStart)) {
                    blankStart = timeline->getTrackById_const(trackId)->getBlankEnd(blankStart) + 1;
                } else {
                    blankStart = start + timeline->getItemPlaytime(cid);
                }
            }
            int nextBlank = timeline->getTrackById_const(trackId)->getNextBlankStart(blankStart);
            if (nextBlank == blankStart) {
                blankStart = timeline->getTrackById_const(trackId)->getBlankEnd(blankStart) + 1;
                nextBlank = timeline->getTrackById_const(trackId)->getNextBlankStart(blankStart);
                if (nextBlank == blankStart) {
                    break;
                }
            }
            if (nextBlank < blankStart) {
                // Done
                break;
            }
            blankStart = nextBlank;
        }
    }
    pCore->pushUndo(undo, redo, i18n("Remove space on track"));
    return true;
}

bool TimelineFunctions::requestDeleteAllClipsFrom(const std::shared_ptr<TimelineItemModel> &timeline, int trackId, int position)
{
    // Abort if track is locked
    if (timeline->trackIsLocked(trackId)) {
        timeline->flashLock(trackId);
        return false;
    }
    // Start undoable command
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    std::unordered_set<int> items;
    if (timeline->isSubtitleTrack(trackId)) {
        // Subtitle track
        items = timeline->getSubtitleModel()->getItemsInRange(position, -1);
    } else {
        items = timeline->getTrackById_const(trackId)->getClipsInRange(position, -1);
    }
    if (items.size() == 0) {
        return false;
    }
    for (int id : items) {
        timeline->requestItemDeletion(id, undo, redo);
    }
    pCore->pushUndo(undo, redo, i18n("Delete clips on track"));
    return true;
}

QDomDocument TimelineFunctions::extractClip(const std::shared_ptr<TimelineItemModel> &timeline, int cid, const QString &binId)
{
    int tid = timeline->getClipTrackId(cid);
    int pos = timeline->getClipPosition(cid);
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(binId);
    QDomDocument sourceDoc;
    QDomDocument destDoc;
    if (!Xml::docContentFromFile(sourceDoc, clip->clipUrl(), false)) {
        return destDoc;
    }
    QDomElement container = destDoc.createElement(QStringLiteral("kdenlive-scene"));
    destDoc.appendChild(container);
    QDomElement bin = destDoc.createElement(QStringLiteral("bin"));
    container.appendChild(bin);
    bool isAudio = timeline->isAudioTrack(tid);
    container.setAttribute(QStringLiteral("offset"), pos);
    container.setAttribute(QStringLiteral("documentid"), QStringLiteral("000000"));
    // Process producers
    QList<int> processedProducers;
    QString blackBg;
    QMap<QString, int> producerMap;
    QMap<QString, double> producerSpeed;
    QMap<QString, int> producerSpeedResource;
    QDomNodeList producers = sourceDoc.elementsByTagName(QLatin1String("producer"));
    for (int i = 0; i < producers.count(); ++i) {
        QDomElement currentProd = producers.item(i).toElement();
        bool ok;
        int clipId = Xml::getXmlProperty(currentProd, QLatin1String("kdenlive:id")).toInt(&ok);
        if (!ok) {
            // Check if this is a black bg track
            if (Xml::hasXmlProperty(currentProd, QLatin1String("kdenlive:playlistid"))) {
                // This is the black bg track
                blackBg = currentProd.attribute(QStringLiteral("id"));
                continue;
            }
            const QString resource = Xml::getXmlProperty(currentProd, QLatin1String("resource"));
            qDebug() << "===== CLIP NOT FOUND: " << resource;
            if (producerSpeedResource.contains(resource)) {
                clipId = producerSpeedResource.value(resource);
                qDebug() << "===== GOT PREVIOUS ID: " << clipId;
                QString baseProducerId;
                int baseProducerClipId = 0;
                QMapIterator<QString, int> m(producerMap);
                while (m.hasNext()) {
                    m.next();
                    if (m.value() == clipId) {
                        baseProducerId = m.key();
                        baseProducerClipId = m.value();
                        qDebug() << "=== FOUND PRODUCER FOR ID: " << m.key();
                        break;
                    }
                }
                if (!baseProducerId.isEmpty()) {
                    producerSpeed.insert(currentProd.attribute(QLatin1String("id")), producerSpeed.value(baseProducerId));
                    producerMap.insert(currentProd.attribute(QLatin1String("id")), baseProducerClipId);
                    qDebug() << "/// INSERTING PRODUCERMAP: " << currentProd.attribute(QLatin1String("id")) << "=" << baseProducerClipId;
                }
                // Producer already processed;
                continue;
            } else {
                clipId = pCore->projectItemModel()->getFreeClipId();
            }
            Xml::setXmlProperty(currentProd, QStringLiteral("kdenlive:id"), QString::number(clipId));
            qDebug() << "=== UNKNOWN CLIP FOUND: " << Xml::getXmlProperty(currentProd, QLatin1String("resource"));
        }
        producerMap.insert(currentProd.attribute(QLatin1String("id")), clipId);
        qDebug() << "/// INSERTING SOURCE PRODUCERMAP: " << currentProd.attribute(QLatin1String("id")) << "=" << clipId;
        QString mltService = Xml::getXmlProperty(currentProd, QStringLiteral("mlt_service"));
        if (mltService == QLatin1String("timewarp")) {
            // Speed producer
            double speed = Xml::getXmlProperty(currentProd, QStringLiteral("warp_speed")).toDouble();
            Xml::setXmlProperty(currentProd, QStringLiteral("mlt_service"), QStringLiteral("avformat"));
            producerSpeedResource.insert(Xml::getXmlProperty(currentProd, QLatin1String("resource")), clipId);
            qDebug() << "===== CLIP SPEED RESOURCE: " << Xml::getXmlProperty(currentProd, QLatin1String("resource")) << " = " << clipId;
            QString resource = Xml::getXmlProperty(currentProd, QStringLiteral("warp_resource"));
            Xml::setXmlProperty(currentProd, QStringLiteral("resource"), resource);
            producerSpeed.insert(currentProd.attribute(QLatin1String("id")), speed);
        }
        if (processedProducers.contains(clipId)) {
            // Producer already processed
            continue;
        }
        processedProducers << clipId;
        // This could be a timeline track producer, reset custom audio/video setting
        Xml::removeXmlProperty(currentProd, QLatin1String("set.test_audio"));
        Xml::removeXmlProperty(currentProd, QLatin1String("set.test_image"));
        bin.appendChild(destDoc.importNode(currentProd, true));
    }
    // Same for chains
    QDomNodeList chains = sourceDoc.elementsByTagName(QStringLiteral("chain"));
    for (int i = 0; i < chains.count(); ++i) {
        QDomElement currentProd = chains.item(i).toElement();
        bool ok;
        int clipId = Xml::getXmlProperty(currentProd, QLatin1String("kdenlive:id")).toInt(&ok);
        if (!ok) {
            const QString resource = Xml::getXmlProperty(currentProd, QLatin1String("resource"));
            qDebug() << "===== CLIP NOT FOUND: " << resource;
            if (producerSpeedResource.contains(resource)) {
                clipId = producerSpeedResource.value(resource);
                qDebug() << "===== GOT PREVIOUS ID: " << clipId;
                QString baseProducerId;
                int baseProducerClipId = 0;
                QMapIterator<QString, int> m(producerMap);
                while (m.hasNext()) {
                    m.next();
                    if (m.value() == clipId) {
                        baseProducerId = m.key();
                        baseProducerClipId = m.value();
                        qDebug() << "=== FOUND PRODUCER FOR ID: " << m.key();
                        break;
                    }
                }
                if (!baseProducerId.isEmpty()) {
                    producerSpeed.insert(currentProd.attribute(QLatin1String("id")), producerSpeed.value(baseProducerId));
                    producerMap.insert(currentProd.attribute(QLatin1String("id")), baseProducerClipId);
                    qDebug() << "/// INSERTING PRODUCERMAP: " << currentProd.attribute(QLatin1String("id")) << "=" << baseProducerClipId;
                }
                // Producer already processed;
                continue;
            } else {
                clipId = pCore->projectItemModel()->getFreeClipId();
            }
            Xml::setXmlProperty(currentProd, QStringLiteral("kdenlive:id"), QString::number(clipId));
            qDebug() << "=== UNKNOWN CLIP FOUND: " << Xml::getXmlProperty(currentProd, QLatin1String("resource"));
        }
        producerMap.insert(currentProd.attribute(QLatin1String("id")), clipId);
        qDebug() << "/// INSERTING SOURCE PRODUCERMAP: " << currentProd.attribute(QLatin1String("id")) << "=" << clipId;
        QString mltService = Xml::getXmlProperty(currentProd, QStringLiteral("mlt_service"));
        if (mltService == QLatin1String("timewarp")) {
            // Speed producer
            double speed = Xml::getXmlProperty(currentProd, QStringLiteral("warp_speed")).toDouble();
            Xml::setXmlProperty(currentProd, QStringLiteral("mlt_service"), QStringLiteral("avformat"));
            producerSpeedResource.insert(Xml::getXmlProperty(currentProd, QLatin1String("resource")), clipId);
            qDebug() << "===== CLIP SPEED RESOURCE: " << Xml::getXmlProperty(currentProd, QLatin1String("resource")) << " = " << clipId;
            QString resource = Xml::getXmlProperty(currentProd, QStringLiteral("warp_resource"));
            Xml::setXmlProperty(currentProd, QStringLiteral("resource"), resource);
            producerSpeed.insert(currentProd.attribute(QLatin1String("id")), speed);
        }
        if (processedProducers.contains(clipId)) {
            // Producer already processed
            continue;
        }
        processedProducers << clipId;
        // This could be a timeline track producer, reset custom audio/video setting
        Xml::removeXmlProperty(currentProd, QLatin1String("set.test_audio"));
        Xml::removeXmlProperty(currentProd, QLatin1String("set.test_image"));
        bin.appendChild(destDoc.importNode(currentProd, true));
    }
    // Check for audio tracks
    QMap<QString, bool> tracksType;
    int audioTracks = 0;
    int videoTracks = 0;
    QDomNodeList tracks = sourceDoc.elementsByTagName(QLatin1String("track"));
    for (int i = 0; i < tracks.count(); ++i) {
        QDomElement currentTrack = tracks.item(i).toElement();
        if (currentTrack.attribute(QLatin1String("hide")) == QLatin1String("video")) {
            // Audio track
            tracksType.insert(currentTrack.attribute(QLatin1String("producer")), true);
            audioTracks++;
        } else {
            // Video track
            if (!blackBg.isEmpty() && blackBg == currentTrack.attribute(QLatin1String("producer"))) {
                continue;
            }
            tracksType.insert(currentTrack.attribute(QLatin1String("producer")), false);
            videoTracks++;
        }
    }
    int track = 1;
    if (isAudio) {
        container.setAttribute(QStringLiteral("masterAudioTrack"), 0);
    } else {
        track = audioTracks;
        container.setAttribute(QStringLiteral("masterTrack"), track);
    }
    // Process playlists
    QDomNodeList playlists = sourceDoc.elementsByTagName(QLatin1String("playlist"));
    for (int i = 0; i < playlists.count(); ++i) {
        QDomElement currentPlay = playlists.item(i).toElement();
        int position = 0;
        bool audioTrack = tracksType.value(currentPlay.attribute("id"));
        if (audioTrack != isAudio) {
            continue;
        }
        QDomNodeList elements = currentPlay.childNodes();
        for (int j = 0; j < elements.count(); ++j) {
            QDomElement currentElement = elements.item(j).toElement();
            if (currentElement.tagName() == QLatin1String("blank")) {
                position += currentElement.attribute(QLatin1String("length")).toInt();
                continue;
            }
            if (currentElement.tagName() == QLatin1String("entry")) {
                QDomElement clipElement = destDoc.createElement(QStringLiteral("clip"));
                container.appendChild(clipElement);
                int in = currentElement.attribute(QLatin1String("in")).toInt();
                int out = currentElement.attribute(QLatin1String("out")).toInt();
                const QString originalProducer = currentElement.attribute(QLatin1String("producer"));
                clipElement.setAttribute(QLatin1String("binid"), producerMap.value(originalProducer));
                clipElement.setAttribute(QLatin1String("in"), in);
                clipElement.setAttribute(QLatin1String("out"), out);
                clipElement.setAttribute(QLatin1String("position"), position + pos);
                clipElement.setAttribute(QLatin1String("track"), track);
                // clipElement.setAttribute(QStringLiteral("state"), (int)m_currentState);
                clipElement.setAttribute(QStringLiteral("state"), audioTrack ? 2 : 1);
                if (audioTrack) {
                    clipElement.setAttribute(QLatin1String("audioTrack"), 1);
                    int mirror = audioTrack + videoTracks - track - 1;
                    if (track <= videoTracks) {
                        clipElement.setAttribute(QLatin1String("mirrorTrack"), mirror);
                    } else {
                        clipElement.setAttribute(QLatin1String("mirrorTrack"), -1);
                    }
                }
                if (producerSpeed.contains(originalProducer)) {
                    clipElement.setAttribute(QStringLiteral("speed"), producerSpeed.value(originalProducer));
                } else {
                    clipElement.setAttribute(QStringLiteral("speed"), 1);
                }
                position += (out - in + 1);
                QDomNodeList effects = currentElement.elementsByTagName(QLatin1String("filter"));
                if (effects.count() == 0) {
                    continue;
                }
                QDomElement effectsList = destDoc.createElement(QStringLiteral("effects"));
                clipElement.appendChild(effectsList);
                effectsList.setAttribute(QStringLiteral("parentIn"), in);
                for (int k = 0; k < effects.count(); ++k) {
                    QDomElement effect = effects.item(k).toElement();
                    QString filterId = Xml::getXmlProperty(effect, QLatin1String("kdenlive_id"));
                    QDomElement clipEffect = destDoc.createElement(QStringLiteral("effect"));
                    effectsList.appendChild(clipEffect);
                    clipEffect.setAttribute(QStringLiteral("id"), filterId);
                    QDomNodeList properties = effect.childNodes();
                    if (effect.hasAttribute(QStringLiteral("in"))) {
                        clipEffect.setAttribute(QStringLiteral("in"), effect.attribute(QStringLiteral("in")));
                    }
                    if (effect.hasAttribute(QStringLiteral("out"))) {
                        clipEffect.setAttribute(QStringLiteral("out"), effect.attribute(QStringLiteral("out")));
                    }
                    for (int l = 0; l < properties.count(); ++l) {
                        QDomElement prop = properties.item(l).toElement();
                        const QString propName = prop.attribute(QLatin1String("name"));
                        if (propName == QLatin1String("mlt_service") || propName == QLatin1String("kdenlive_id")) {
                            continue;
                        }
                        Xml::setXmlProperty(clipEffect, propName, prop.text());
                    }
                }
            }
        }
        track++;
    }
    if (!isAudio) {
        // Compositions
        QDomNodeList compositions = sourceDoc.elementsByTagName(QLatin1String("transition"));
        for (int i = 0; i < compositions.count(); ++i) {
            QDomElement currentCompo = compositions.item(i).toElement();
            if (Xml::getXmlProperty(currentCompo, QLatin1String("internal_added")).toInt() > 0) {
                // Track compositing, discard
                continue;
            }
            QDomElement composition = destDoc.createElement(QStringLiteral("composition"));
            container.appendChild(composition);
            int in = currentCompo.attribute(QLatin1String("in")).toInt();
            int out = currentCompo.attribute(QLatin1String("out")).toInt();
            const QString compoId = Xml::getXmlProperty(currentCompo, QLatin1String("kdenlive_id"));
            composition.setAttribute(QLatin1String("position"), in + pos);
            composition.setAttribute(QLatin1String("in"), in);
            composition.setAttribute(QLatin1String("out"), out);
            composition.setAttribute(QLatin1String("composition"), compoId);
            int a_track = Xml::getXmlProperty(currentCompo, QLatin1String("a_track")).toInt();
            int b_track = Xml::getXmlProperty(currentCompo, QLatin1String("b_track")).toInt();
            if (!blackBg.isEmpty()) {
                a_track--;
                b_track--;
            }
            composition.setAttribute(QLatin1String("a_track"), a_track);
            composition.setAttribute(QLatin1String("track"), b_track);
            QDomNodeList properties = currentCompo.childNodes();
            for (int l = 0; l < properties.count(); ++l) {
                QDomElement prop = properties.item(l).toElement();
                const QString &propName = prop.attribute(QLatin1String("name"));
                Xml::setXmlProperty(composition, propName, prop.text());
            }
        }
    }
    qDebug() << "=== GOT CONVERTED DOCUMENT\n\n" << destDoc.toString();
    return destDoc;
}

int TimelineFunctions::spacerMinPos()
{
    return spacerMinPosition;
}
