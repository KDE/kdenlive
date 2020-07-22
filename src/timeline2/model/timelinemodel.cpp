/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "timelinemodel.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "clipmodel.hpp"
#include "compositionmodel.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "groupsmodel.hpp"
#include "kdenlivesettings.h"
#include "logger.hpp"
#include "snapmodel.hpp"
#include "timelinefunctions.hpp"
#include "trackmodel.hpp"

#include <QDebug>
#include <QThread>
#include <QModelIndex>
#include <klocalizedstring.h>
#include <mlt++/MltConsumer.h>
#include <mlt++/MltField.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>
#include <mlt++/MltTransition.h>
#include <queue>

#include "macros.hpp"

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
    registration::class_<TimelineModel>("TimelineModel")
        .method("setTrackLockedState", &TimelineModel::setTrackLockedState)(parameter_names("trackId", "lock"))
        .method("requestClipMove", select_overload<bool(int, int, int, bool, bool, bool, bool)>(&TimelineModel::requestClipMove))(
            parameter_names("clipId", "trackId", "position", "moveMirrorTracks", "updateView", "logUndo", "invalidateTimeline"))
        .method("requestCompositionMove", select_overload<bool(int, int, int, bool, bool)>(&TimelineModel::requestCompositionMove))(
            parameter_names("compoId", "trackId", "position", "updateView", "logUndo"))
        .method("requestClipInsertion", select_overload<bool(const QString &, int, int, int &, bool, bool, bool)>(&TimelineModel::requestClipInsertion))(
            parameter_names("binClipId", "trackId", "position", "id", "logUndo", "refreshView", "useTargets"))
        .method("requestItemDeletion", select_overload<bool(int, bool)>(&TimelineModel::requestItemDeletion))(parameter_names("clipId", "logUndo"))
        .method("requestGroupMove", select_overload<bool(int, int, int, int, bool, bool, bool)>(&TimelineModel::requestGroupMove))(
            parameter_names("itemId", "groupId", "delta_track", "delta_pos", "moveMirrorTracks", "updateView", "logUndo"))
        .method("requestGroupDeletion", select_overload<bool(int, bool)>(&TimelineModel::requestGroupDeletion))(parameter_names("clipId", "logUndo"))
        .method("requestItemResize", select_overload<int(int, int, bool, bool, int, bool)>(&TimelineModel::requestItemResize))(
            parameter_names("itemId", "size", "right", "logUndo", "snapDistance", "allowSingleResize"))
        .method("requestClipsGroup", select_overload<int(const std::unordered_set<int> &, bool, GroupType)>(&TimelineModel::requestClipsGroup))(
            parameter_names("itemIds", "logUndo", "type"))
        .method("requestClipUngroup", select_overload<bool(int, bool)>(&TimelineModel::requestClipUngroup))(parameter_names("itemId", "logUndo"))
        .method("requestClipsUngroup", &TimelineModel::requestClipsUngroup)(parameter_names("itemIds", "logUndo"))
        .method("requestTrackInsertion", select_overload<bool(int, int &, const QString &, bool)>(&TimelineModel::requestTrackInsertion))(
            parameter_names("pos", "id", "trackName", "audioTrack"))
        .method("requestTrackDeletion", select_overload<bool(int)>(&TimelineModel::requestTrackDeletion))(parameter_names("trackId"))
        .method("requestClearSelection", select_overload<bool(bool)>(&TimelineModel::requestClearSelection))(parameter_names("onDeletion"))
        .method("requestAddToSelection", &TimelineModel::requestAddToSelection)(parameter_names("itemId", "clear"))
        .method("requestRemoveFromSelection", &TimelineModel::requestRemoveFromSelection)(parameter_names("itemId"))
        .method("requestSetSelection", select_overload<bool(const std::unordered_set<int> &)>(&TimelineModel::requestSetSelection))(parameter_names("itemIds"))
        .method("requestFakeClipMove", select_overload<bool(int, int, int, bool, bool, bool)>(&TimelineModel::requestFakeClipMove))(
            parameter_names("clipId", "trackId", "position", "updateView", "logUndo", "invalidateTimeline"))
        .method("requestFakeGroupMove", select_overload<bool(int, int, int, int, bool, bool)>(&TimelineModel::requestFakeGroupMove))(
            parameter_names("clipId", "groupId", "delta_track", "delta_pos", "updateView", "logUndo"))
        .method("suggestClipMove", &TimelineModel::suggestClipMove)(parameter_names("clipId", "trackId", "position", "cursorPosition", "snapDistance", "moveMirrorTracks"))
        .method("suggestCompositionMove",
                &TimelineModel::suggestCompositionMove)(parameter_names("compoId", "trackId", "position", "cursorPosition", "snapDistance"))
        // .method("addSnap", &TimelineModel::addSnap)(parameter_names("pos"))
        // .method("removeSnap", &TimelineModel::addSnap)(parameter_names("pos"))
        // .method("requestCompositionInsertion", select_overload<bool(const QString &, int, int, int, std::unique_ptr<Mlt::Properties>, int &, bool)>(
        //                                            &TimelineModel::requestCompositionInsertion))(
        //     parameter_names("transitionId", "trackId", "position", "length", "transProps", "id", "logUndo"))
        .method("requestClipTimeWarp", select_overload<bool(int, double,bool,bool)>(&TimelineModel::requestClipTimeWarp))(parameter_names("clipId", "speed","pitchCompensate","changeDuration"));
}

int TimelineModel::next_id = 0;
int TimelineModel::seekDuration = 30000;

TimelineModel::TimelineModel(Mlt::Profile *profile, std::weak_ptr<DocUndoStack> undo_stack)
    : QAbstractItemModel_shared_from_this()
    , m_blockRefresh(false)
    , m_tractor(new Mlt::Tractor(*profile))
    , m_masterStack(nullptr)
    , m_snaps(new SnapModel())
    , m_undoStack(std::move(undo_stack))
    , m_profile(profile)
    , m_blackClip(new Mlt::Producer(*profile, "color:black"))
    , m_lock(QReadWriteLock::Recursive)
    , m_timelineEffectsEnabled(true)
    , m_id(getNextId())
    , m_overlayTrackCount(-1)
    , m_videoTarget(-1)
    , m_editMode(TimelineMode::NormalEdit)
    , m_closing(false)
{
    // Create black background track
    m_blackClip->set("id", "black_track");
    m_blackClip->set("mlt_type", "producer");
    m_blackClip->set("aspect_ratio", 1);
    m_blackClip->set("length", INT_MAX);
    m_blackClip->set("mlt_image_format", "rgb24a");
    m_blackClip->set("set.test_audio", 0);
    m_blackClip->set_in_and_out(0, TimelineModel::seekDuration);
    m_tractor->insert_track(*m_blackClip, 0);

    TRACE_CONSTR(this);
}

void TimelineModel::prepareClose()
{
    requestClearSelection(true);
    QWriteLocker locker(&m_lock);
    // Unlock all tracks to allow deleting clip from tracks
    m_closing = true;
    auto it = m_allTracks.begin();
    while (it != m_allTracks.end()) {
        (*it)->unlock();
        ++it;
    }
}

TimelineModel::~TimelineModel()
{
    std::vector<int> all_ids;
    for (auto tracks : m_iteratorTable) {
        all_ids.push_back(tracks.first);
    }
    for (auto tracks : all_ids) {
        deregisterTrack_lambda(tracks)();
    }
    for (const auto &clip : m_allClips) {
        clip.second->deregisterClipToBin();
    }
}

int TimelineModel::getTracksCount() const
{
    READ_LOCK();
    int count = m_tractor->count();
    if (m_overlayTrackCount > -1) {
        count -= m_overlayTrackCount;
    }
    Q_ASSERT(count >= 0);
    // don't count the black background track
    Q_ASSERT(count - 1 == static_cast<int>(m_allTracks.size()));
    return count - 1;
}

QPair<int, int> TimelineModel::getAVtracksCount() const
{
    QPair <int, int> tracks{0, 0};
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack()) {
            tracks.second++;
        } else {
            tracks.first++;
        }
        ++it;
    }
    if (m_overlayTrackCount > -1) {
        tracks.first -= m_overlayTrackCount;
    }
    return tracks;
}

QList<int> TimelineModel::getTracksIds(bool audio) const
{
    QList <int> trackIds;
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack() == audio) {
            trackIds.insert(-1, (*it)->getId());
        }
        ++it;
    }
    return trackIds;
}

int TimelineModel::getTrackIndexFromPosition(int pos) const
{
    Q_ASSERT(pos >= 0 && pos < (int)m_allTracks.size());
    READ_LOCK();
    auto it = m_allTracks.cbegin();
    while (pos > 0) {
        it++;
        pos--;
    }
    return (*it)->getId();
}

int TimelineModel::getClipsCount() const
{
    READ_LOCK();
    int size = int(m_allClips.size());
    return size;
}

int TimelineModel::getCompositionsCount() const
{
    READ_LOCK();
    int size = int(m_allCompositions.size());
    return size;
}

int TimelineModel::getClipTrackId(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    return clip->getCurrentTrackId();
}

int TimelineModel::getCompositionTrackId(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    const auto trans = m_allCompositions.at(compoId);
    return trans->getCurrentTrackId();
}

int TimelineModel::getItemTrackId(int itemId) const
{
    READ_LOCK();
    Q_ASSERT(isItem(itemId));
    if (isComposition(itemId)) {
        return getCompositionTrackId(itemId);
    }
    return getClipTrackId(itemId);
}

int TimelineModel::getClipPosition(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    int pos = clip->getPosition();
    return pos;
}

double TimelineModel::getClipSpeed(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId)->getSpeed();
}

int TimelineModel::getClipSplitPartner(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_groups->getSplitPartner(clipId);
}

int TimelineModel::getClipIn(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    return clip->getIn();
}

PlaylistState::ClipState TimelineModel::getClipState(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    return clip->clipState();
}

const QString TimelineModel::getClipBinId(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    QString id = clip->binId();
    return id;
}

int TimelineModel::getClipPlaytime(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(clipId));
    const auto clip = m_allClips.at(clipId);
    int playtime = clip->getPlaytime();
    return playtime;
}

QSize TimelineModel::getClipFrameSize(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(clipId));
    const auto clip = m_allClips.at(clipId);
    return clip->getFrameSize();
}

int TimelineModel::getTrackClipsCount(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    int count = getTrackById_const(trackId)->getClipsCount();
    return count;
}

int TimelineModel::getClipByPosition(int trackId, int position) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->getClipByPosition(position);
}

int TimelineModel::getCompositionByPosition(int trackId, int position) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->getCompositionByPosition(position);
}

int TimelineModel::getTrackPosition(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_allTracks.cbegin();
    int pos = (int)std::distance(it, (decltype(it))m_iteratorTable.at(trackId));
    return pos;
}

int TimelineModel::getTrackMltIndex(int trackId) const
{
    READ_LOCK();
    // Because of the black track that we insert in first position, the mlt index is the position + 1
    return getTrackPosition(trackId) + 1;
}

int TimelineModel::getTrackSortValue(int trackId, int separated) const
{
    if (separated == 1) {
        // This will be A2, A1, V1, V2
        return getTrackPosition(trackId) + 1;
    }
    if (separated == 2) {
        // This will be A1, A2, V1, V2
        // Count audio/video tracks
        auto it = m_allTracks.cbegin();
        int aCount = 0;
        int vCount = 0;
        int refPos = 0;
        bool isVideo = true;
        while (it != m_allTracks.cend()) {
            if ((*it)->isAudioTrack()) {
                if ((*it)->getId() == trackId) {
                    refPos = aCount;
                    isVideo = false;
                }
                aCount++;
            } else {
                // video track
                if ((*it)->getId() == trackId) {
                    refPos = vCount;
                }
                vCount++;
            }
            ++it;
        }
        return isVideo ? aCount + refPos + 1 : aCount - refPos;
    }
    // This will be A1, V1, A2, V2
    auto it = m_allTracks.cend();
    int aCount = 0;
    int vCount = 0;
    bool isAudio = false;
    int trackPos = 0;
    while (it != m_allTracks.begin()) {
        --it;
        bool audioTrack = (*it)->isAudioTrack();
        if (audioTrack) {
            aCount++;
        } else {
            vCount++;
        }
        if (trackId == (*it)->getId()) {
            isAudio = audioTrack;
            trackPos = audioTrack ? aCount : vCount;
        }
    }
    int trackDiff = qMax(0, aCount - vCount);
    if (trackDiff > 0) {
        // more audio tracks, keep them below
        if (isAudio && trackPos > vCount) {
            return -trackPos;
        }
    }
    return isAudio ? 2 * trackPos : 2 * (vCount + 1 - trackPos) + 1;
}

QList<int> TimelineModel::getLowerTracksId(int trackId, TrackType type) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    QList<int> results;
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.cbegin()) {
        --it;
        if (type == TrackType::AnyTrack) {
            results << (*it)->getId();
            continue;
        }
        bool audioTrack = (*it)->isAudioTrack();
        if (type == TrackType::AudioTrack && audioTrack) {
            results << (*it)->getId();
        } else if (type == TrackType::VideoTrack && !audioTrack) {
            results << (*it)->getId();
        }
    }
    return results;
}

int TimelineModel::getPreviousVideoTrackIndex(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.cbegin()) {
        --it;
        if (!(*it)->isAudioTrack()) {
            return (*it)->getId();
        }
    }
    return 0;
}

int TimelineModel::getPreviousVideoTrackPos(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.cbegin()) {
        --it;
        if (!(*it)->isAudioTrack()) {
            return getTrackMltIndex((*it)->getId());
        }
    }
    return 0;
}

int TimelineModel::getMirrorVideoTrackId(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    if (!(*it)->isAudioTrack()) {
        // we expected an audio track...
        return -1;
    }
    int count = 0;
    while (it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack()) {
            count++;
        } else {
            count--;
            if (count == 0) {
                return (*it)->getId();
            }
        }
        ++it;
    }
    return -1;
}

int TimelineModel::getMirrorTrackId(int trackId) const
{
    if (isAudioTrack(trackId)) {
        return getMirrorVideoTrackId(trackId);
    }
    return getMirrorAudioTrackId(trackId);
}

int TimelineModel::getMirrorAudioTrackId(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    if ((*it)->isAudioTrack()) {
        // we expected a video track...
        qDebug()<<"++++++++\n+++++++ ERROR RQSTNG AUDIO MIRROR FOR AUDIO";
        return -1;
    }
    int count = 0;
    while (it != m_allTracks.cbegin()) {
        if (!(*it)->isAudioTrack()) {
            count++;
        } else {
            count--;
            if (count == 0) {
                return (*it)->getId();
            }
        }
        --it;
    }
    if ((*it)->isAudioTrack() && count == 1) {
        return (*it)->getId();
    }
    return -1;
}

void TimelineModel::setEditMode(TimelineMode::EditMode mode)
{
    m_editMode = mode;
}

bool TimelineModel::normalEdit() const
{
    return m_editMode == TimelineMode::NormalEdit;
}

bool TimelineModel::requestFakeClipMove(int clipId, int trackId, int position, bool updateView, bool invalidateTimeline, Fun &undo, Fun &redo)
{
    Q_UNUSED(updateView);
    Q_UNUSED(invalidateTimeline);
    Q_UNUSED(undo);
    Q_UNUSED(redo);
    Q_ASSERT(isClip(clipId));
    m_allClips[clipId]->setFakePosition(position);
    bool trackChanged = false;
    if (trackId > -1) {
        if (trackId != m_allClips[clipId]->getFakeTrackId()) {
            if (getTrackById_const(trackId)->trackType() == m_allClips[clipId]->clipState()) {
                m_allClips[clipId]->setFakeTrackId(trackId);
                trackChanged = true;
            }
        }
    }
    QModelIndex modelIndex = makeClipIndexFromID(clipId);
    if (modelIndex.isValid()) {
        QVector<int> roles{FakePositionRole};
        if (trackChanged) {
            roles << FakeTrackIdRole;
        }
        notifyChange(modelIndex, modelIndex, roles);
        return true;
    }
    return false;
}

bool TimelineModel::requestClipMove(int clipId, int trackId, int position, bool moveMirrorTracks, bool updateView, bool invalidateTimeline, bool finalMove, Fun &undo, Fun &redo, bool groupMove)
{
    Q_UNUSED(moveMirrorTracks)
    // qDebug() << "// FINAL MOVE: " << invalidateTimeline << ", UPDATE VIEW: " << updateView<<", FINAL: "<<finalMove;
    if (trackId == -1) {
        return false;
    }
    Q_ASSERT(isClip(clipId));
    if (m_allClips[clipId]->clipState() == PlaylistState::Disabled) {
        if (getTrackById_const(trackId)->trackType() == PlaylistState::AudioOnly && !m_allClips[clipId]->canBeAudio()) {
            return false;
        }
        if (getTrackById_const(trackId)->trackType() == PlaylistState::VideoOnly && !m_allClips[clipId]->canBeVideo()) {
            return false;
        }
    } else if (getTrackById_const(trackId)->trackType() != m_allClips[clipId]->clipState()) {
        // Move not allowed (audio / video mismatch)
        //qDebug() << "// CLIP MISMATC FOR TK: "<<trackId<< ", " << getTrackById_const(trackId)->trackType() << " == " << m_allClips[clipId]->clipState();
        return false;
    }
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    bool ok = true;
    int old_trackId = getClipTrackId(clipId);
    bool notifyViewOnly = false;
    // qDebug()<<"MOVING CLIP FROM: "<<old_trackId<<" == "<<trackId;
    Fun update_model = []() { return true; };
    if (old_trackId == trackId) {
        // Move on same track, simply inform the view
        updateView = false;
        notifyViewOnly = true;
        update_model = [clipId, this, trackId, invalidateTimeline]() {
            QModelIndex modelIndex = makeClipIndexFromID(clipId);
            notifyChange(modelIndex, modelIndex, StartRole);
            if (invalidateTimeline && !getTrackById_const(trackId)->isAudioTrack()) {
                int in = getClipPosition(clipId);
                emit invalidateZone(in, in + getClipPlaytime(clipId));
            }
            return true;
        };
    }
    if (old_trackId != -1) {
        if (notifyViewOnly) {
            PUSH_LAMBDA(update_model, local_undo);
        }
        ok = getTrackById(old_trackId)->requestClipDeletion(clipId, updateView, finalMove, local_undo, local_redo, groupMove, false);
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    ok = getTrackById(trackId)->requestClipInsertion(clipId, position, updateView, finalMove, local_undo, local_redo, groupMove);
    if (!ok) {
        qDebug() << "-------------\n\nINSERTION FAILED, REVERTING\n\n-------------------";
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    update_model();
    if (notifyViewOnly) {
        PUSH_LAMBDA(update_model, local_redo);
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestFakeClipMove(int clipId, int trackId, int position, bool updateView, bool logUndo, bool invalidateTimeline)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, trackId, position, updateView, logUndo, invalidateTimeline)
    Q_ASSERT(m_allClips.count(clipId) > 0);
    if (m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(clipId);
        int current_trackId = getClipTrackId(clipId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allClips[clipId]->getPosition();
        bool res = requestFakeGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo);
        TRACE_RES(res);
        return res;
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestFakeClipMove(clipId, trackId, position, updateView, invalidateTimeline, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move clip"));
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestClipMove(int clipId, int trackId, int position, bool moveMirrorTracks, bool updateView, bool logUndo, bool invalidateTimeline)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, trackId, position, updateView, logUndo, invalidateTimeline);
    Q_ASSERT(m_allClips.count(clipId) > 0);
    if (m_allClips[clipId]->getPosition() == position && getClipTrackId(clipId) == trackId) {
        TRACE_RES(true);
        return true;
    }
    if (m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(clipId);
        int current_trackId = getClipTrackId(clipId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allClips[clipId]->getPosition();
        return requestGroupMove(clipId, groupId, delta_track, delta_pos, moveMirrorTracks, updateView, logUndo);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestClipMove(clipId, trackId, position, moveMirrorTracks, updateView, invalidateTimeline, logUndo, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move clip"));
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestClipMoveAttempt(int clipId, int trackId, int position)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allClips.count(clipId) > 0);
    if (m_allClips[clipId]->getPosition() == position && getClipTrackId(clipId) == trackId) {
        return true;
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = true;
    if (m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(clipId);
        int current_trackId = getClipTrackId(clipId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allClips[clipId]->getPosition();
        res = requestGroupMove(clipId, groupId, delta_track, delta_pos, false, false, undo, redo, false);
    } else {
        res = requestClipMove(clipId, trackId, position, true, false, false, false, undo, redo);
    }
    if (res) {
        undo();
    }
    return res;
}

QVariantList TimelineModel::suggestItemMove(int itemId, int trackId, int position, int cursorPosition, int snapDistance)
{
    if (isClip(itemId)) {
        return suggestClipMove(itemId, trackId, position, cursorPosition, snapDistance);
    }
    return suggestCompositionMove(itemId, trackId, position, cursorPosition, snapDistance);
}

int TimelineModel::adjustFrame(int frame, int trackId)
{
    if (m_editMode == TimelineMode::InsertEdit && isTrack(trackId)) {
        frame = qMin(frame, getTrackById_const(trackId)->trackDuration());
    }
    return frame;
}

QVariantList TimelineModel::suggestClipMove(int clipId, int trackId, int position, int cursorPosition, int snapDistance, bool moveMirrorTracks)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, trackId, position, cursorPosition, snapDistance);
    Q_ASSERT(isClip(clipId));
    Q_ASSERT(isTrack(trackId));
    int currentPos = m_editMode == TimelineMode::NormalEdit ? getClipPosition(clipId) : m_allClips[clipId]->getFakePosition();
    int offset = m_editMode == TimelineMode::NormalEdit ? 0 : getClipPosition(clipId) - currentPos;
    int sourceTrackId = (m_editMode != TimelineMode::NormalEdit) ? m_allClips[clipId]->getFakeTrackId() : getClipTrackId(clipId);
    if (sourceTrackId > -1 && getTrackById_const(trackId)->isAudioTrack() != getTrackById_const(sourceTrackId)->isAudioTrack()) {
        // Trying move on incompatible track type, stay on same track
        trackId = sourceTrackId;
    }
    if (currentPos == position && sourceTrackId == trackId) {
        TRACE_RES(position);
        return {position, trackId};
    }
    if (m_editMode == TimelineMode::InsertEdit) {
        int maxPos = getTrackById_const(trackId)->trackDuration();
        if (m_allClips[clipId]->getCurrentTrackId() == trackId) {
            maxPos -= m_allClips[clipId]->getPlaytime();
        }
        qDebug()<<"=== TRYING CLIP MOVE AT: "<<position<<", MAX TK: "<<getTrackById_const(trackId)->trackDuration();
        position = qMin(position, maxPos);
    }
    bool after = position > currentPos;
    if (snapDistance > 0) {
        std::vector<int> ignored_pts;
        // For snapping, we must ignore all in/outs of the clips of the group being moved
        std::unordered_set<int> all_items = {clipId};
        if (m_groups->isInGroup(clipId)) {
            int groupId = m_groups->getRootId(clipId);
            all_items = m_groups->getLeaves(groupId);
        }
        for (int current_clipId : all_items) {
            if (getItemTrackId(current_clipId) != -1) {
                if (isClip(current_clipId)) {
                    m_allClips[current_clipId]->allSnaps(ignored_pts, offset);
                } else {
                    // Composition
                    int in = getItemPosition(current_clipId) - offset;
                    ignored_pts.push_back(in);
                    ignored_pts.push_back(in + getItemPlaytime(current_clipId));
                }
            }
        }
        int snapped = getBestSnapPos(currentPos, position - currentPos, ignored_pts, cursorPosition, snapDistance);
        // qDebug() << "Starting suggestion " << clipId << position << currentPos << "snapped to " << snapped;
        if (snapped >= 0) {
            position = snapped;
        }
    }
    // we check if move is possible
    bool possible = (m_editMode == TimelineMode::NormalEdit) ? requestClipMove(clipId, trackId, position, moveMirrorTracks, true, false, false)
                                                           : requestFakeClipMove(clipId, trackId, position, true, false, false);
    if (possible) {
        TRACE_RES(position);
        if (m_editMode != TimelineMode::NormalEdit) {
            trackId = m_allClips[clipId]->getFakeTrackId();
        }
        return {position, trackId};
    }
    if (sourceTrackId == -1) {
        // not clear what to do here, if the current move doesn't work. We could try to find empty space, but it might end up being far away...
        TRACE_RES(currentPos);
        return {currentPos, -1};
    }
    // Find best possible move
    if (!m_groups->isInGroup(clipId)) {
        // Try same track move
        if (trackId != sourceTrackId && sourceTrackId != -1) {
            trackId = sourceTrackId;
            possible = requestClipMove(clipId, trackId, position, moveMirrorTracks, true, false, false);
            if (!possible) {
                qDebug() << "CANNOT MOVE CLIP : " << clipId << " ON TK: " << trackId << ", AT POS: " << position;
            } else {
                TRACE_RES(position);
                return {position, trackId};
            }
        }

        int blank_length = getTrackById_const(trackId)->getBlankSizeNearClip(clipId, after);
        qDebug() << "Found blank" << blank_length;
        if (blank_length < INT_MAX) {
            if (after) {
                position = currentPos + blank_length;
            } else {
                position = currentPos - blank_length;
            }
        } else {
            TRACE_RES(currentPos);
            return {currentPos, sourceTrackId};
        }
        possible = requestClipMove(clipId, trackId, position, moveMirrorTracks, true, false, false);
        TRACE_RES(possible ? position : currentPos);
        if (possible) {
            return {position, trackId};
        }
        return {currentPos, sourceTrackId};
    }
    if (trackId != sourceTrackId) {
        // Try same track move
        possible = requestClipMove(clipId, sourceTrackId, position, moveMirrorTracks, true, false, false);
        if (possible) {
            return {position, sourceTrackId};
        }
        return {currentPos, sourceTrackId};
    }
    // find best pos for groups
    int groupId = m_groups->getRootId(clipId);
    std::unordered_set<int> all_items = m_groups->getLeaves(groupId);
    QMap<int, int> trackPosition;

    // First pass, sort clips by track and keep only the first / last depending on move direction
    for (int current_clipId : all_items) {
        int clipTrack = getItemTrackId(current_clipId);
        if (clipTrack == -1) {
            continue;
        }
        int in = getItemPosition(current_clipId);
        if (trackPosition.contains(clipTrack)) {
            if (after) {
                // keep only last clip position for track
                int out = in + getItemPlaytime(current_clipId);
                if (trackPosition.value(clipTrack) < out) {
                    trackPosition.insert(clipTrack, out);
                }
            } else {
                // keep only first clip position for track
                if (trackPosition.value(clipTrack) > in) {
                    trackPosition.insert(clipTrack, in);
                }
            }
        } else {
            trackPosition.insert(clipTrack, after ? in + getItemPlaytime(current_clipId) - 1 : in);
        }
    }

    // Now check space on each track
    QMapIterator<int, int> i(trackPosition);
    int blank_length = 0;
    while (i.hasNext()) {
        i.next();
        int track_space;
        if (!after) {
            // Check space before the position
            track_space = i.value() - getTrackById_const(i.key())->getBlankStart(i.value() - 1);
            if (blank_length == 0 || blank_length > track_space) {
                blank_length = track_space;
            }
        } else {
            // Check space after the position
            track_space = getTrackById(i.key())->getBlankEnd(i.value() + 1) - i.value() - 1;
            if (blank_length == 0 || blank_length > track_space) {
                blank_length = track_space;
            }
        }
    }
    if (snapDistance > 0) {
        if (blank_length > 10 * snapDistance) {
            blank_length = 0;
        }
    } else if (blank_length / m_profile->fps() > 5) {
        blank_length = 0;
    }
    if (blank_length != 0) {
        int updatedPos = currentPos + (after ? blank_length : -blank_length);
        possible = requestClipMove(clipId, trackId, updatedPos, moveMirrorTracks, true, false, false);
        if (possible) {
            TRACE_RES(updatedPos);
            return {updatedPos, trackId};
        }
    }
    TRACE_RES(currentPos);
    return {currentPos, sourceTrackId};
}

QVariantList TimelineModel::suggestCompositionMove(int compoId, int trackId, int position, int cursorPosition, int snapDistance)
{
    QWriteLocker locker(&m_lock);
    TRACE(compoId, trackId, position, cursorPosition, snapDistance);
    Q_ASSERT(isComposition(compoId));
    Q_ASSERT(isTrack(trackId));
    int currentPos = getCompositionPosition(compoId);
    int currentTrack = getCompositionTrackId(compoId);
    if (getTrackById_const(trackId)->isAudioTrack()) {
        // Trying move on incompatible track type, stay on same track
        trackId = currentTrack;
    }
    if (currentPos == position && currentTrack == trackId) {
        TRACE_RES(position);
        return {position, trackId};
    }

    if (snapDistance > 0) {
        // For snapping, we must ignore all in/outs of the clips of the group being moved
        std::vector<int> ignored_pts;
        if (m_groups->isInGroup(compoId)) {
            int groupId = m_groups->getRootId(compoId);
            auto all_items = m_groups->getLeaves(groupId);
            for (int current_compoId : all_items) {
                // TODO: fix for composition
                int in = getItemPosition(current_compoId);
                ignored_pts.push_back(in);
                ignored_pts.push_back(in + getItemPlaytime(current_compoId));
            }
        } else {
            int in = currentPos;
            int out = in + getCompositionPlaytime(compoId);
            qDebug() << " * ** IGNORING SNAP PTS: " << in << "-" << out;
            ignored_pts.push_back(in);
            ignored_pts.push_back(out);
        }
        int snapped = getBestSnapPos(currentPos, position - currentPos, ignored_pts, cursorPosition, snapDistance);
        qDebug() << "Starting suggestion " << compoId << position << currentPos << "snapped to " << snapped;
        if (snapped >= 0) {
            position = snapped;
        }
    }
    // we check if move is possible
    bool possible = requestCompositionMove(compoId, trackId, position, true, false);
    qDebug() << "Original move success" << possible;
    if (possible) {
        TRACE_RES(position);
        return {position, trackId};
    }
    /*bool after = position > currentPos;
    int blank_length = getTrackById(trackId)->getBlankSizeNearComposition(compoId, after);
    qDebug() << "Found blank" << blank_length;
    if (blank_length < INT_MAX) {
        if (after) {
            return currentPos + blank_length;
        }
        return currentPos - blank_length;
    }
    return position;*/
    TRACE_RES(currentPos);
    return {currentPos, currentTrack};
}

bool TimelineModel::requestClipCreation(const QString &binClipId, int &id, PlaylistState::ClipState state, int audioStream, double speed, bool warp_pitch, Fun &undo, Fun &redo)
{
    qDebug() << "requestClipCreation " << binClipId;
    QString bid = binClipId;
    if (binClipId.contains(QLatin1Char('/'))) {
        bid = binClipId.section(QLatin1Char('/'), 0, 0);
    }
    if (!pCore->projectItemModel()->hasClip(bid)) {
        qDebug() << " / / / /MASTER CLIP NOT FOUND";
        return false;
    }
    std::shared_ptr<ProjectClip> master = pCore->projectItemModel()->getClipByBinID(bid);
    if (!master->isReady() || !master->isCompatible(state)) {
        qDebug() << "// CLIP NOT READY OR NOT COMPATIBLE: " << state<<" - - READY: "<<master->isReady();
        return false;
    }
    int clipId = TimelineModel::getNextId();
    id = clipId;
    Fun local_undo = deregisterClip_lambda(clipId);
    ClipModel::construct(shared_from_this(), bid, clipId, state, audioStream, speed, warp_pitch);
    auto clip = m_allClips[clipId];
    Fun local_redo = [clip, this, state, audioStream, speed, warp_pitch]() {
        // We capture a shared_ptr to the clip, which means that as long as this undo object lives, the clip object is not deleted. To insert it back it is
        // sufficient to register it.
        registerClip(clip, true);
        clip->refreshProducerFromBin(-1, state, audioStream, speed, warp_pitch);
        return true;
    };

    if (binClipId.contains(QLatin1Char('/'))) {
        int in = binClipId.section(QLatin1Char('/'), 1, 1).toInt();
        int out = binClipId.section(QLatin1Char('/'), 2, 2).toInt();
        int initLength = m_allClips[clipId]->getPlaytime();
        bool res = true;
        if (in != 0) {
            res = requestItemResize(clipId, initLength - in, false, true, local_undo, local_redo);
        }
        res = res && requestItemResize(clipId, out - in + 1, true, true, local_undo, local_redo);
        if (!res) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestClipInsertion(const QString &binClipId, int trackId, int position, int &id, bool logUndo, bool refreshView, bool useTargets)
{
    QWriteLocker locker(&m_lock);
    TRACE(binClipId, trackId, position, id, logUndo, refreshView, useTargets);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestClipInsertion(binClipId, trackId, position, id, logUndo, refreshView, useTargets, undo, redo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Insert Clip"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestClipInsertion(const QString &binClipId, int trackId, int position, int &id, bool logUndo, bool refreshView, bool useTargets,
                                         Fun &undo, Fun &redo, QVector<int> allowedTracks)
{
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    qDebug() << "requestClipInsertion " << binClipId << " "
             << " " << trackId << " " << position;
    bool res = false;
    ClipType::ProducerType type = ClipType::Unknown;
    // binClipId id is in the form: A2/10/50
    // A2 means audio only insertion for bin clip with id 2
    // 10 is in point
    // 50 is out point
    QString binIdWithInOut = binClipId;
    // bid is the A2 part
    QString bid = binClipId.section(QLatin1Char('/'), 0, 0);
    // dropType indicates if we want a normal drop (disabled), audio only or video only drop
    PlaylistState::ClipState dropType = PlaylistState::Disabled;
    if (bid.startsWith(QLatin1Char('A'))) {
        dropType = PlaylistState::AudioOnly;
        bid.remove(0, 1);
        binIdWithInOut.remove(0, 1);
    } else if (bid.startsWith(QLatin1Char('V'))) {
        dropType = PlaylistState::VideoOnly;
        bid.remove(0, 1);
        binIdWithInOut.remove(0, 1);
    }
    if (!pCore->projectItemModel()->hasClip(bid)) {
        return false;
    }

    bool audioDrop = false;
    if (!useTargets) {
        audioDrop = getTrackById_const(trackId)->isAudioTrack();
        if (audioDrop) {
            if (dropType == PlaylistState::VideoOnly) {
                return false;
            }
        } else if (dropType == PlaylistState::AudioOnly) {
            return false;
        }
    }

    std::shared_ptr<ProjectClip> master = pCore->projectItemModel()->getClipByBinID(bid);
    type = master->clipType();
    if (useTargets && m_audioTarget.isEmpty() && m_videoTarget == -1) {
        useTargets = false;
    }
    if ((dropType == PlaylistState::Disabled || dropType == PlaylistState::AudioOnly) && (type == ClipType::AV || type == ClipType::Playlist)) {
        if (useTargets && !m_audioTarget.isEmpty() && m_videoTarget == -1) {
            // If audio target is set but no video target, only insert audio
            trackId = m_audioTarget.firstKey();
            if (trackId > -1 && (getTrackById_const(trackId)->isLocked() || !allowedTracks.contains(trackId))) {
                trackId = -1;
            }
        } else if (useTargets && (getTrackById_const(trackId)->isLocked() || !allowedTracks.contains(trackId))) {
            // Video target set but locked
            trackId = m_audioTarget.firstKey();
            if (trackId > -1 && (getTrackById_const(trackId)->isLocked() || !allowedTracks.contains(trackId))) {
                trackId = -1;
            }
        }
        if (trackId == -1) {
            if (!allowedTracks.isEmpty()) {
                // No active tracks, aborting
                return true;
            }
            pCore->displayMessage(i18n("No available track for insert operation"), ErrorMessage);
            return false;
        }
        int audioStream = -1;
        QList <int> keys = m_binAudioTargets.keys();
        if (!useTargets) {
            // Drag and drop, calculate target tracks
            if (audioDrop) {
                if (keys.count() > 1) {
                    // Dropping a clip with several audio streams
                    int tracksBelow = getLowerTracksId(trackId, TrackType::AudioTrack).count();
                    if (tracksBelow < keys.count() - 1) {
                        // We don't have enough audio tracks below, check above
                        QList <int> audioTrackIds = getTracksIds(true);
                        qDebug()<<"==== GOT AUIO TIDS: "<<audioTrackIds;
                        if (audioTrackIds.count() < keys.count()) {
                            // Not enough audio tracks
                            pCore->displayMessage(i18n("Not enough audio tracks for all streams (%1)", keys.count()), ErrorMessage);
                            return false;
                        }
                        trackId = audioTrackIds.at(audioTrackIds.count() - keys.count());
                    }
                }
                if (keys.isEmpty()) {
                    return false;
                }
                audioStream = keys.first();
            } else {
                // Dropping video, ensure we have enough audio tracks for its streams
                int mirror = getMirrorTrackId(trackId);
                QList <int> audioTids = {};
                if (mirror > -1) {
                   audioTids = getLowerTracksId(mirror, TrackType::AudioTrack);
                }
                if (audioTids.count() < keys.count() - 1 || (mirror == -1 && !keys.isEmpty())) {
                    // Check if project has enough audio tracks
                    if (keys.count() > getTracksIds(true).count()) {
                        // Not enough audio tracks in the project
                        pCore->displayMessage(i18n("Not enough audio tracks for all streams (%1)", keys.count()), ErrorMessage);
                    }
                    return false;
                }
            }
        } else if (audioDrop) {
            // Using our targets
            audioStream = m_audioTarget.first();
        }

        res = requestClipCreation(binIdWithInOut, id, getTrackById_const(trackId)->trackType(), audioDrop ? audioStream : -1, 1.0, false, local_undo, local_redo);
        res = res && requestClipMove(id, trackId, position, true, refreshView, logUndo, logUndo, local_undo, local_redo);
        qDebug()<<"==== INSERTED FIRST AUDIO STREAM: "<<audioStream<<", ON TK: "<<trackId<<"\n-----";
        QList <int> target_track;
        if (audioDrop) {
            if (m_videoTarget > -1 && !getTrackById_const(m_videoTarget)->isLocked() && dropType != PlaylistState::AudioOnly) {
                target_track << m_videoTarget;
            }
        } else if (useTargets) {
            QList <int> targetIds = m_audioTarget.keys();
            for (int &ix : targetIds) {
                if (!getTrackById_const(ix)->isLocked() && allowedTracks.contains(ix)) {
                    target_track << ix;
                }
            }
        }
        qDebug() << "CLIP HAS A+V: " << master->hasAudioAndVideo();
        // Get mirror track
        int mirror = dropType == PlaylistState::Disabled ? getMirrorTrackId(trackId) : -1;
        if (mirror > -1 && getTrackById_const(mirror)->isLocked()) {
            mirror = -1;
        }
        bool canMirrorDrop = !useTargets && ((mirror > -1 && (audioDrop || !keys.isEmpty())) || keys.count() > 1);
        QMap<int, int> dropTargets;
        if (res && (canMirrorDrop || !target_track.isEmpty()) && master->hasAudioAndVideo()) {
            int streamsCount = 0;
            if (!useTargets) {
                target_track.clear();
                QList <int> audioTids;
                if (!audioDrop) {
                    // insert audio mirror track
                    target_track << mirror;
                    audioTids = getLowerTracksId(mirror, TrackType::AudioTrack);
                } else {
                    qDebug()<<"=== AUDIO DROP; BELOW TK: "<<trackId;
                    audioTids = getLowerTracksId(trackId, TrackType::AudioTrack);
                }
                // First audio stream already inserted in target_track or in timeline
                streamsCount = m_binAudioTargets.count() - 1;
                qDebug()<<"=== GOT AUDIO STRAMS: "<<streamsCount<<"\nAUDIO IDS: "<<audioTids;
                while (streamsCount > 0 && !audioTids.isEmpty()) {
                    target_track << audioTids.takeFirst();
                    streamsCount--;
                }
                QList <int> aTargets = m_binAudioTargets.keys();
                if (audioDrop) {
                    aTargets.removeAll(audioStream);
                }
                //qDebug()<<"========\nPOSSIBLE TARGET TKS: "<<target_track<<"\nRQSTED AUDIO TARGETS: "<<aTargets<<"\n===========";
                std::sort(aTargets.begin(), aTargets.end());
                for (int i = 0; i < target_track.count() && i < aTargets.count() ; ++i) {
                    dropTargets.insert(target_track.at(i), aTargets.at(i));
                }
                if (audioDrop && mirror > -1) {
                    target_track << mirror;
                }
                //qDebug()<<"==== GOT DROP AUDIO TARGETS: "<<dropTargets<<", FINAL TARGET TKS: "<<target_track;
            }
            if (target_track.isEmpty()) {
                // No available track for splitting, abort
                pCore->displayMessage(i18n("No available track for split operation"), ErrorMessage);
                res = false;
            }
            if (!res) {
                bool undone = local_undo();
                Q_ASSERT(undone);
                id = -1;
                return false;
            }
            // Process all mirror insertions
            std::function<bool(void)> audio_undo = []() { return true; };
            std::function<bool(void)> audio_redo = []() { return true; };
            std::unordered_set<int> createdMirrors = {id};
            int mirrorAudioStream = -1;
            for (int &target_ix : target_track) {
                qDebug()<<"=== TESTING MIRRONR ON TK: "<<target_ix;
                bool currentDropIsAudio = !audioDrop;
                if (!useTargets && m_binAudioTargets.count() > 1 && dropTargets.contains(target_ix)) {
                    // Audio clip dropped first but has other streams
                    currentDropIsAudio = true;
                    mirrorAudioStream = dropTargets.value(target_ix);
                    qDebug()<<"==== TESTING STREAM DROP: "<<mirrorAudioStream;
                    if (mirrorAudioStream == audioStream) {
                        qDebug()<<"==== TESTING STREAM DROP ABORTED ";
                        continue;
                    }
                }
                else if (currentDropIsAudio) {
                    if (!useTargets) {
                        mirrorAudioStream = dropTargets.value(target_ix);
                        qDebug()<<"==== TESTING OTHER STREAM DROP: "<<mirrorAudioStream;
                    } else {
                        mirrorAudioStream = m_audioTarget.value(target_ix);
                    }
                } else {
                    qDebug()<<"=== DROPPING VIDEO, STREAMS: "<<streamsCount;
                }
                int newId;
                res = requestClipCreation(binIdWithInOut, newId, currentDropIsAudio ? PlaylistState::AudioOnly : PlaylistState::VideoOnly, currentDropIsAudio ? mirrorAudioStream : -1, 1.0, false, audio_undo, audio_redo);
                if (res) {
                    res = requestClipMove(newId, target_ix, position, true, true, true, true, audio_undo, audio_redo);
                    // use lazy evaluation to group only if move was successful
                    if (!res) {
                        pCore->displayMessage(i18n("Audio split failed: no viable track"), ErrorMessage);
                        bool undone = audio_undo();
                        Q_ASSERT(undone);
                        break;
                    } else {
                        createdMirrors.insert(newId);
                    }
                } else {
                    pCore->displayMessage(i18n("Audio split failed: impossible to create audio clip"), ErrorMessage);
                    bool undone = audio_undo();
                    Q_ASSERT(undone);
                    break;
                }
            }
            if (res) {
                requestClipsGroup(createdMirrors, audio_undo, audio_redo, GroupType::AVSplit);
                UPDATE_UNDO_REDO(audio_redo, audio_undo, local_undo, local_redo);
            }
        }
    } else {
        std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(bid);
        if (dropType == PlaylistState::Disabled) {
            dropType = getTrackById_const(trackId)->trackType();
        } else if (dropType != getTrackById_const(trackId)->trackType()) {
            qDebug() << "// INCORRECT DRAG, ABORTING";
            return false;
        }
        QString normalisedBinId = binClipId;
        if (normalisedBinId.startsWith(QLatin1Char('A')) || normalisedBinId.startsWith(QLatin1Char('V'))) {
            normalisedBinId.remove(0, 1);
        }
        res = requestClipCreation(normalisedBinId, id, dropType, binClip->getProducerIntProperty(QStringLiteral("audio_index")), 1.0, false, local_undo, local_redo);
        res = res && requestClipMove(id, trackId, position, true, refreshView, logUndo, logUndo, local_undo, local_redo);
    }
    if (!res) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        id = -1;
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestItemDeletion(int itemId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (m_groups->isInGroup(itemId)) {
        return requestGroupDeletion(itemId, undo, redo);
    }
    if (isClip(itemId)) {
        return requestClipDeletion(itemId, undo, redo);
    }
    if (isComposition(itemId)) {
        return requestCompositionDeletion(itemId, undo, redo);
    }
    Q_ASSERT(false);
    return false;
}

bool TimelineModel::requestItemDeletion(int itemId, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, logUndo);
    Q_ASSERT(isItem(itemId));
    QString actionLabel;
    if (m_groups->isInGroup(itemId)) {
        actionLabel = i18n("Remove group");
    } else {
        if (isClip(itemId)) {
            actionLabel = i18n("Delete Clip");
        } else {
            actionLabel = i18n("Delete Composition");
        }
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestItemDeletion(itemId, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, actionLabel);
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestClipDeletion(int clipId, Fun &undo, Fun &redo)
{
    int trackId = getClipTrackId(clipId);
    if (trackId != -1) {
        bool res = getTrackById(trackId)->requestClipDeletion(clipId, true, true, undo, redo, false, true);
        if (!res) {
            undo();
            return false;
        }
    }
    auto operation = deregisterClip_lambda(clipId);
    auto clip = m_allClips[clipId];
    Fun reverse = [this, clip]() {
        // We capture a shared_ptr to the clip, which means that as long as this undo object lives, the clip object is not deleted. To insert it back it is
        // sufficient to register it.
        registerClip(clip, true);
        return true;
    };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    undo();
    return false;
}

bool TimelineModel::requestCompositionDeletion(int compositionId, Fun &undo, Fun &redo)
{
    int trackId = getCompositionTrackId(compositionId);
    if (trackId != -1) {
        bool res = getTrackById(trackId)->requestCompositionDeletion(compositionId, true, true, undo, redo, true);
        if (!res) {
            undo();
            return false;
        } else {
            Fun unplant_op = [this, compositionId]() {
                unplantComposition(compositionId);
                return true;
            };
            unplant_op();
            PUSH_LAMBDA(unplant_op, redo);
        }
    }
    Fun operation = deregisterComposition_lambda(compositionId);
    auto composition = m_allCompositions[compositionId];
    int new_in = composition->getPosition();
    int new_out = new_in + composition->getPlaytime();
    Fun reverse = [this, composition, compositionId, trackId, new_in, new_out]() {
        // We capture a shared_ptr to the composition, which means that as long as this undo object lives, the composition object is not deleted. To insert it
        // back it is sufficient to register it.
        registerComposition(composition);
        composition->setCurrentTrackId(trackId, true);
        replantCompositions(compositionId, false);
        checkRefresh(new_in, new_out);
        return true;
    };
    if (operation()) {
        Fun update_monitor = [this, new_in, new_out]() {
            checkRefresh(new_in, new_out);
            return true;
        };
        update_monitor();
        PUSH_LAMBDA(update_monitor, operation);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    undo();
    return false;
}

std::unordered_set<int> TimelineModel::getItemsInRange(int trackId, int start, int end, bool listCompositions)
{
    Q_UNUSED(listCompositions)

    std::unordered_set<int> allClips;
    if (trackId == -1) {
        for (const auto &track : m_allTracks) {
            if (track->isLocked()) {
                continue;
            }
            std::unordered_set<int> clipTracks = getItemsInRange(track->getId(), start, end, listCompositions);
            allClips.insert(clipTracks.begin(), clipTracks.end());
        }
    } else {
        std::unordered_set<int> clipTracks = getTrackById(trackId)->getClipsInRange(start, end);
        allClips.insert(clipTracks.begin(), clipTracks.end());
        if (listCompositions) {
            std::unordered_set<int> compoTracks = getTrackById(trackId)->getCompositionsInRange(start, end);
            allClips.insert(compoTracks.begin(), compoTracks.end());
        }
    }
    return allClips;
}

bool TimelineModel::requestFakeGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool logUndo)
{
    TRACE(clipId, groupId, delta_track, delta_pos, updateView, logUndo);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestFakeGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move group"));
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestFakeGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool finalMove, Fun &undo, Fun &redo,
                                         bool allowViewRefresh)
{
    Q_UNUSED(updateView);
    Q_UNUSED(finalMove);
    Q_UNUSED(undo);
    Q_UNUSED(redo);
    Q_UNUSED(allowViewRefresh);
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allGroups.count(groupId) > 0);
    bool ok = true;
    auto all_items = m_groups->getLeaves(groupId);
    Q_ASSERT(all_items.size() > 1);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };

    // Moving groups is a two stage process: first we remove the clips from the tracks, and then try to insert them back at their calculated new positions.
    // This way, we ensure that no conflict will arise with clips inside the group being moved

    // Check if there is a track move

    // First, remove clips
    bool hasAudio = false;
    bool hasVideo = false;
    std::unordered_map<int, int> old_track_ids, old_position, old_forced_track;
    for (int item : all_items) {
        int old_trackId = getItemTrackId(item);
        old_track_ids[item] = old_trackId;
        if (old_trackId != -1) {
            if (isClip(item)) {
                old_position[item] = m_allClips[item]->getPosition();
                if (!hasAudio && getTrackById_const(old_trackId)->isAudioTrack()) {
                    hasAudio = true;
                } else if (!hasVideo && !getTrackById_const(old_trackId)->isAudioTrack()) {
                    hasVideo = true;
                }
            } else {
                hasVideo = true;
                old_position[item] = m_allCompositions[item]->getPosition();
                old_forced_track[item] = m_allCompositions[item]->getForcedTrack();
            }
        }
    }

    // Second step, calculate delta
    int audio_delta, video_delta;
    audio_delta = video_delta = delta_track;

    if (getTrackById(old_track_ids[clipId])->isAudioTrack()) {
        // Master clip is audio, so reverse delta for video clips
        if (hasAudio) {
            video_delta = -delta_track;
        } else {
            video_delta = 0;
        }
    } else {
        if (hasVideo) {
            audio_delta = -delta_track;
        } else {
            audio_delta = 0;
        }
    }
    bool trackChanged = false;

    // Reverse sort. We need to insert from left to right to avoid confusing the view
    for (int item : all_items) {
        int current_track_id = old_track_ids[item];
        int current_track_position = getTrackPosition(current_track_id);
        int d = getTrackById_const(current_track_id)->isAudioTrack() ? audio_delta : video_delta;
        int target_track_position = current_track_position + d;
        if (target_track_position >= 0 && target_track_position < getTracksCount()) {
            auto it = m_allTracks.cbegin();
            std::advance(it, target_track_position);
            int target_track = (*it)->getId();
            int target_position = old_position[item] + delta_pos;
            if (isClip(item)) {
                qDebug() << "/// SETTING FAKE CLIP: " << target_track << ", POSITION: " << target_position;
                m_allClips[item]->setFakePosition(target_position);
                if (m_allClips[item]->getFakeTrackId() != target_track) {
                    trackChanged = true;
                }
                m_allClips[item]->setFakeTrackId(target_track);
            } else {
            }
        } else {
            qDebug() << "// ABORTING; MOVE TRIED ON TRACK: " << target_track_position << "..\n..\n..";
            ok = false;
        }
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    QModelIndex modelIndex;
    QVector<int> roles{FakePositionRole};
    if (trackChanged) {
        roles << FakeTrackIdRole;
    }
    for (int item : all_items) {
        if (isClip(item)) {
            modelIndex = makeClipIndexFromID(item);
        } else {
            modelIndex = makeCompositionIndexFromID(item);
        }
        notifyChange(modelIndex, modelIndex, roles);
    }
    return true;
}

bool TimelineModel::requestGroupMove(int itemId, int groupId, int delta_track, int delta_pos, bool moveMirrorTracks, bool updateView, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, groupId, delta_track, delta_pos, updateView, logUndo);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestGroupMove(itemId, groupId, delta_track, delta_pos, updateView, logUndo, undo, redo, moveMirrorTracks);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move group"));
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestGroupMove(int itemId, int groupId, int delta_track, int delta_pos, bool updateView, bool finalMove, Fun &undo, Fun &redo, bool moveMirrorTracks,
                                     bool allowViewRefresh, QVector<int> allowedTracks)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allGroups.count(groupId) > 0);
    Q_ASSERT(isItem(itemId));
    if (getGroupElements(groupId).count(itemId) == 0) {
        // this group doesn't contain the clip, abort
        return false;
    }
    bool ok = true;
    auto all_items = m_groups->getLeaves(groupId);
    Q_ASSERT(all_items.size() > 1);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    std::vector< std::pair<int, int> > sorted_clips;
    std::vector< std::pair<int, std::pair<int, int> > > sorted_compositions;
    int lowerTrack = -1;
    int upperTrack = -1;

    // Separate clips from compositions to sort and check source tracks
    for (int affectedItemId : all_items) {
        if (delta_track != 0) {
            // Check if an upper / lower move is possible
            const int trackPos = getTrackPosition(getItemTrackId(affectedItemId));
            if (lowerTrack == -1 || lowerTrack > trackPos) {
                lowerTrack = trackPos;
            }
            if (upperTrack == -1 || upperTrack < trackPos) {
                upperTrack = trackPos;
            }
        }
        if (isClip(affectedItemId)) {
            sorted_clips.push_back({affectedItemId, m_allClips[affectedItemId]->getPosition()});
        } else {
            sorted_compositions.push_back({affectedItemId, {m_allCompositions[affectedItemId]->getPosition(), getTrackMltIndex(m_allCompositions[affectedItemId]->getCurrentTrackId())}});
        }
    }

    // Sort clips first
    std::sort(sorted_clips.begin(), sorted_clips.end(), [delta_pos](const std::pair<int, int> &clipId1, const std::pair<int, int> &clipId2) {
        return delta_pos > 0 ? clipId2.second < clipId1.second : clipId1.second < clipId2.second;
    });

    // Sort compositions. We need to delete in the move direction from top to bottom
    std::sort(sorted_compositions.begin(), sorted_compositions.end(), [delta_track, delta_pos](const std::pair<int, std::pair<int, int > > &clipId1, const std::pair<int, std::pair<int, int > > &clipId2) {
        const int p1 = delta_track < 0
                     ? clipId1.second.second : delta_track > 0 ? -clipId1.second.second : clipId1.second.first;
        const int p2 = delta_track < 0
                     ? clipId2.second.second : delta_track > 0 ? -clipId2.second.second : clipId2.second.first;
        return delta_track == 0 ? (delta_pos > 0 ? p2 < p1 : p1 < p2) : p1 < p2;
    });

    // Moving groups is a two stage process: first we remove the clips from the tracks, and then try to insert them back at their calculated new positions.
    // This way, we ensure that no conflict will arise with clips inside the group being moved

    Fun update_model = [this, finalMove]() { 
        if (finalMove) {
            updateDuration();
        }
        return true;
    };

    // Check if there is a track move
    bool updatePositionOnly = false;
    // Second step, reinsert clips at correct positions
    int audio_delta, video_delta;
    audio_delta = video_delta = delta_track;
    bool masterIsAudio = getTrackById_const(getItemTrackId(itemId))->isAudioTrack();
    if (delta_track < 0) {
        if (!masterIsAudio) {
            // Case 1, dragging a video clip down
            bool lowerTrackIsAudio = getTrackById_const(getTrackIndexFromPosition(lowerTrack))->isAudioTrack();
            int lowerPos = lowerTrackIsAudio ? lowerTrack - delta_track : lowerTrack + delta_track;
            if (lowerPos < 0) {
                // No space below
                delta_track = 0;
            } else if (!lowerTrackIsAudio) {
                // Moving a group of video clips
                if (getTrackById_const(getTrackIndexFromPosition(lowerPos))->isAudioTrack()) {
                    // Moving to a non matching track (video on audio track)
                    delta_track = 0;
                }
            }
        } else if (lowerTrack + delta_track < 0) {
            // Case 2, dragging an audio clip down
            delta_track = 0;
        }
    } else if (delta_track > 0) {
        if (!masterIsAudio) {
            // Case 1, dragging a video clip up
            int upperPos = upperTrack + delta_track;
            if (upperPos >= getTracksCount()) {
                // Moving above top track, not allowed
                delta_track = 0;
            } else if (getTrackById_const(getTrackIndexFromPosition(upperPos))->isAudioTrack()) {
                // Trying to move to a non matching track (video clip on audio track)
                delta_track = 0;
            }
        } else {
            bool upperTrackIsAudio = getTrackById_const(getTrackIndexFromPosition(upperTrack))->isAudioTrack();
            if (!upperTrackIsAudio) {
                // Dragging an audio clip up, check that upper video clip has an available video track
                int targetPos = upperTrack - delta_track;
                if (moveMirrorTracks && (targetPos <0 || getTrackById_const(getTrackIndexFromPosition(targetPos))->isAudioTrack())) {
                    delta_track = 0;
                }
            } else {
                int targetPos = upperTrack + delta_track;
                if (targetPos >= getTracksCount() || !getTrackById_const(getTrackIndexFromPosition(targetPos))->isAudioTrack()) {
                    // Trying to drag audio above topmost track or on video track
                    delta_track = 0;
                }
            }
        }
    }
    if (delta_track == 0 && updateView) {
        updateView = false;
        allowViewRefresh = false;
        updatePositionOnly = true;
        update_model = [sorted_clips, sorted_compositions, finalMove, this]() {
            QModelIndex modelIndex;
            QVector<int> roles{StartRole};
            for (const std::pair<int, int> &item : sorted_clips) {
                modelIndex = makeClipIndexFromID(item.first);
                notifyChange(modelIndex, modelIndex, roles);
            }
            for (const std::pair<int, std::pair<int, int>> &item : sorted_compositions) {
                modelIndex = makeCompositionIndexFromID(item.first);
                notifyChange(modelIndex, modelIndex, roles);
            }
            if (finalMove) {
                updateDuration();
            }
            return true;
        };
    }

    std::unordered_map<int, int> old_track_ids, old_position, old_forced_track;
    // First, remove clips
    if (delta_track != 0) {
        // We delete our clips only if changing track
        for (const std::pair<int, int> &item : sorted_clips) {
            int old_trackId = getClipTrackId(item.first);
            old_track_ids[item.first] = old_trackId;
            if (old_trackId != -1) {
                bool updateThisView = allowViewRefresh;
                ok = ok && getTrackById(old_trackId)->requestClipDeletion(item.first, updateThisView, finalMove, local_undo, local_redo, true, false);
                old_position[item.first] = item.second;
                if (!ok) {
                    bool undone = local_undo();
                    Q_ASSERT(undone);
                    return false;
                }
            }
        }
        for (const std::pair<int, std::pair<int, int>> &item : sorted_compositions) {
            int old_trackId = getCompositionTrackId(item.first);
            if (old_trackId != -1) {
                old_track_ids[item.first] = old_trackId;
                old_position[item.first] = item.second.first;
                old_forced_track[item.first] = m_allCompositions[item.first]->getForcedTrack();
            }
        }
        if (masterIsAudio) {
            // Master clip is audio, so reverse delta for video clips
            video_delta = -delta_track;
        } else {
            audio_delta = -delta_track;
        }
    }
    // We need to insert depending on the move direction to avoid confusing the view
    // std::reverse(std::begin(sorted_clips), std::end(sorted_clips));
    bool updateThisView = allowViewRefresh;
    if (delta_track == 0) {
        // Special case, we are moving on same track, avoid too many calculations
        // First pass, check for collisions and suggest better delta
        QVector <int> processedTracks;
        for (const std::pair<int, int> &item : sorted_clips) {
            int current_track_id = getClipTrackId(item.first);
            if (processedTracks.contains(current_track_id)) {
                // We only check the first clip for each track since they are sorted depending on the move direction
                continue;
            }
            processedTracks << current_track_id;
            if (!allowedTracks.isEmpty() && !allowedTracks.contains(current_track_id)) {
                continue;
            }
            int current_in = item.second;
            int playtime = getClipPlaytime(item.first);
            int target_position = current_in + delta_pos;
            if (delta_pos < 0) {
                if (!getTrackById_const(current_track_id)->isAvailable(target_position, playtime)) {
                    if (!getTrackById_const(current_track_id)->isBlankAt(current_in - 1)) {
                        // No move possible, abort
                        bool undone = local_undo();
                        Q_ASSERT(undone);
                        return false;
                    }
                    int newStart = getTrackById_const(current_track_id)->getBlankStart(current_in - 1);
                    delta_pos = qMax(delta_pos, newStart - current_in);
                }
            } else {
                int moveEnd = target_position + playtime;
                int moveStart = qMax(current_in + playtime, target_position);
                if (!getTrackById_const(current_track_id)->isAvailable(moveStart, moveEnd - moveStart)) {
                    int newStart = getTrackById_const(current_track_id)->getBlankEnd(current_in + playtime);
                    if (newStart == current_in + playtime) {
                        // No move possible, abort
                        bool undone = local_undo();
                        Q_ASSERT(undone);
                        return false;
                    }
                    delta_pos = qMin(delta_pos, newStart - (current_in + playtime));
                }
            }
        }
        for (const std::pair<int, int> &item : sorted_clips) {
            int current_track_id = getClipTrackId(item.first);
            if (!allowedTracks.isEmpty() && !allowedTracks.contains(current_track_id)) {
                continue;
            }
            int current_in = item.second;
            int target_position = current_in + delta_pos;
            ok = requestClipMove(item.first, current_track_id, target_position, moveMirrorTracks, updateThisView, finalMove, finalMove, local_undo, local_redo, true);
            if (!ok) {
                break;
            }
        }
        if (ok) {
            for (const std::pair<int, std::pair<int, int>> &item : sorted_compositions) {
                int current_track_id = getItemTrackId(item.first);
                if (!allowedTracks.isEmpty() && !allowedTracks.contains(current_track_id)) {
                    continue;
                }
                int current_in = item.second.first;
                int target_position = current_in + delta_pos;
                ok = requestCompositionMove(item.first, current_track_id, m_allCompositions[item.first]->getForcedTrack(), target_position, updateThisView, finalMove, local_undo, local_redo);
                if (!ok) {
                    break;
                }
            }
        }
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    } else {
        // Track changed
        for (const std::pair<int, int> &item : sorted_clips) {
            int current_track_id = old_track_ids[item.first];
            int current_track_position = getTrackPosition(current_track_id);
            int d = getTrackById(current_track_id)->isAudioTrack() ? audio_delta : video_delta;
            if (!moveMirrorTracks && item.first != itemId) {
                d = 0;
            }
            int target_track_position = current_track_position + d;
            if (target_track_position >= 0 && target_track_position < getTracksCount()) {
                auto it = m_allTracks.cbegin();
                std::advance(it, target_track_position);
                int target_track = (*it)->getId();
                int target_position = old_position[item.first] + delta_pos;
                ok = ok && requestClipMove(item.first, target_track, target_position, moveMirrorTracks, updateThisView, finalMove, finalMove, local_undo, local_redo, true);
            } else {
                ok = false;
            }
            if (!ok) {
                bool undone = local_undo();
                Q_ASSERT(undone);
                return false;
            }
        }
        for (const std::pair<int, std::pair<int, int> > &item : sorted_compositions) {
            int current_track_id = old_track_ids[item.first];
            int current_track_position = getTrackPosition(current_track_id);
            int d = getTrackById(current_track_id)->isAudioTrack() ? audio_delta : video_delta;
            int target_track_position = current_track_position + d;

            if (target_track_position >= 0 && target_track_position < getTracksCount()) {
                auto it = m_allTracks.cbegin();
                std::advance(it, target_track_position);
                int target_track = (*it)->getId();
                int target_position = old_position[item.first] + delta_pos;
                ok = ok &&
                     requestCompositionMove(item.first, target_track, old_forced_track[item.first], target_position, updateThisView, finalMove, local_undo, local_redo);
            } else {
                qDebug() << "// ABORTING; MOVE TRIED ON TRACK: " << target_track_position << "..\n..\n..";
                ok = false;
            }
            if (!ok) {
                bool undone = local_undo();
                Q_ASSERT(undone);
                return false;
            }
        }
    }
    if (updatePositionOnly) {
        update_model();
        PUSH_LAMBDA(update_model, local_redo);
        PUSH_LAMBDA(update_model, local_undo);
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestGroupDeletion(int clipId, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, logUndo);
    if (!m_groups->isInGroup(clipId)) {
        TRACE_RES(false);
        return false;
    }
    bool res = requestItemDeletion(clipId, logUndo);
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestGroupDeletion(int clipId, Fun &undo, Fun &redo)
{
    // we do a breadth first exploration of the group tree, ungroup (delete) every inner node, and then delete all the leaves.
    std::queue<int> group_queue;
    group_queue.push(m_groups->getRootId(clipId));
    std::unordered_set<int> all_items;
    std::unordered_set<int> all_compositions;
    while (!group_queue.empty()) {
        int current_group = group_queue.front();
        bool isSelection = m_currentSelection == current_group;
        if (isSelection) {
            m_currentSelection = -1;
        }
        group_queue.pop();
        Q_ASSERT(isGroup(current_group));
        auto children = m_groups->getDirectChildren(current_group);
        int one_child = -1; // we need the id on any of the indices of the elements of the group
        for (int c : children) {
            if (isClip(c)) {
                all_items.insert(c);
                one_child = c;
            } else if (isComposition(c)) {
                all_compositions.insert(c);
                one_child = c;
            } else {
                Q_ASSERT(isGroup(c));
                one_child = c;
                group_queue.push(c);
            }
        }
        if (one_child != -1) {
            if (m_groups->getType(current_group) == GroupType::Selection) {
                Q_ASSERT(isSelection);
                // in the case of a selection group, we delete the group but don't log it in the undo object
                Fun tmp_undo = []() { return true; };
                Fun tmp_redo = []() { return true; };
                m_groups->ungroupItem(one_child, tmp_undo, tmp_redo);
            } else {
                bool res = m_groups->ungroupItem(one_child, undo, redo);
                if (!res) {
                    undo();
                    return false;
                }
            }
        }
    }
    for (int clip : all_items) {
        bool res = requestClipDeletion(clip, undo, redo);
        if (!res) {
            // Undo is processed in requestClipDeletion
            return false;
        }
    }
    for (int compo : all_compositions) {
        bool res = requestCompositionDeletion(compo, undo, redo);
        if (!res) {
            undo();
            return false;
        }
    }
    return true;
}

const QVariantList TimelineModel::getGroupData(int itemId)
{
    QWriteLocker locker(&m_lock);
    if (!m_groups->isInGroup(itemId)) {
        return {itemId, getItemPosition(itemId), getItemPlaytime(itemId)};
    }
    int groupId = m_groups->getRootId(itemId);
    QVariantList result;
    std::unordered_set<int> items = m_groups->getLeaves(groupId);
    for (int id : items) {
        result << id << getItemPosition(id) << getItemPlaytime(id);
    }
    return result;
}

void TimelineModel::processGroupResize(QVariantList startPos, QVariantList endPos, bool right)
{
    Q_ASSERT(startPos.size() == endPos.size());
    QMap<int, QPair<int, int>> startData;
    QMap<int, QPair<int, int>> endData;
    while (!startPos.isEmpty()) {
        int id = startPos.takeFirst().toInt();
        int in = startPos.takeFirst().toInt();
        int duration = startPos.takeFirst().toInt();
        startData.insert(id, {in, duration});
        id = endPos.takeFirst().toInt();
        in = endPos.takeFirst().toInt();
        duration = endPos.takeFirst().toInt();
        endData.insert(id, {in, duration});
    }
    QMapIterator<int, QPair<int, int>> i(startData);
    QList<int> changedItems;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = true;
    while (i.hasNext()) {
        i.next();
        QPair<int, int> startItemPos = i.value();
        QPair<int, int> endItemPos = endData.value(i.key());
        if (startItemPos.first != endItemPos.first || startItemPos.second != endItemPos.second) {
            // Revert individual items to original position
            requestItemResize(i.key(), startItemPos.second, right, false, 0, true);
            changedItems << i.key();
        }
    }
    for (int id : qAsConst(changedItems)) {
        QPair<int, int> endItemPos = endData.value(id);
        result = result & requestItemResize(id, endItemPos.second, right, true, undo, redo, false);
        if (!result) {
            break;
        }
    }
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Resize group"));
    } else {
        undo();
    }
}

const std::vector<int> TimelineModel::getBoundaries(int itemId)
{
    std::vector<int> boundaries;
    std::unordered_set<int> items;
    if (m_groups->isInGroup(itemId)) {
        int groupId = m_groups->getRootId(itemId);
        items = m_groups->getLeaves(groupId);
    } else {
        items.insert(itemId);
    }
    for (int id : items) {
        if (isClip(id) || isComposition(id)) {
            int in = getItemPosition(id);
            int out = in + getItemPlaytime(id);
            boundaries.push_back(in);
            boundaries.push_back(out);
        }
    }
    return boundaries;
}

int TimelineModel::requestClipResizeAndTimeWarp(int itemId, int size, bool right, int snapDistance, bool allowSingleResize, double speed)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, size, right, true, snapDistance, allowSingleResize);
    Q_ASSERT(isClip(itemId));
    if (size <= 0) {
        TRACE_RES(-1);
        return -1;
    }
    int in = getItemPosition(itemId);
    int out = in + getItemPlaytime(itemId);
    //size = requestItemResizeInfo(itemId, in, out, size, right, snapDistance);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::unordered_set<int> all_items;
    if (!allowSingleResize && m_groups->isInGroup(itemId)) {
        int groupId = m_groups->getRootId(itemId);
        std::unordered_set<int> items;
        if (m_groups->getType(groupId) == GroupType::AVSplit) {
            // Only resize group elements if it is an avsplit
            items = m_groups->getLeaves(groupId);
        } else {
            all_items.insert(itemId);
        }
        for (int id : items) {
            if (id == itemId) {
                all_items.insert(id);
                continue;
            }
            int start = getItemPosition(id);
            int end = in + getItemPlaytime(id);
            if (right) {
                if (out == end) {
                    all_items.insert(id);
                }
            } else if (start == in) {
                all_items.insert(id);
            }
        }
    } else {
        all_items.insert(itemId);
    }
    bool result = true;
    for (int id : all_items) {
        int tid = getItemTrackId(id);
        if (tid > -1 && getTrackById_const(tid)->isLocked()) {
            continue;
        }
        // First delete clip, then timewarp, resize and reinsert
        int pos = getItemPosition(id);
        if (!right) {
            pos += getItemPlaytime(id) - size;
        }
        result = getTrackById(tid)->requestClipDeletion(id, true, true, undo, redo, false, true);
        bool pitchCompensate = m_allClips[id]->getIntProperty(QStringLiteral("warp_pitch"));
        result = result && requestClipTimeWarp(id, speed, pitchCompensate, true, undo, redo);
        result = result && requestItemResize(id, size, true, true, undo, redo);
        result = result && getTrackById(tid)->requestClipInsertion(id, pos, true, true, undo, redo);
        if (!result) {
            break;
        }
    }
    if (!result) {
        bool undone = undo();
        Q_ASSERT(undone);
        TRACE_RES(-1);
        return -1;
    }
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Resize clip speed"));
    }
    int res = result ? size : -1;
    TRACE_RES(res);
    return res;
}

int TimelineModel::requestItemResizeInfo(int itemId, int in, int out, int size, bool right, int snapDistance)
{
    if (snapDistance > 0 && getItemTrackId(itemId) != -1) {
        Fun temp_undo = []() { return true; };
        Fun temp_redo = []() { return true; };
        if (right && size > out - in && isClip(itemId)) {
            int targetPos = in + size - 1;
            int trackId = getItemTrackId(itemId);
            if (!getTrackById_const(trackId)->isBlankAt(targetPos)) {
                size = getTrackById_const(trackId)->getBlankEnd(out + 1) - in;
            }
        } else if (!right && size > (out - in) && isClip(itemId)) {
            int targetPos = out - size;
            int trackId = getItemTrackId(itemId);
            if (!getTrackById_const(trackId)->isBlankAt(targetPos)) {
                size = out - getTrackById_const(trackId)->getBlankStart(in - 1);
            }
        }
        int timelinePos = pCore->getTimelinePosition();
        m_snaps->addPoint(timelinePos);
        int proposed_size = m_snaps->proposeSize(in, out, getBoundaries(itemId), size, right, snapDistance);
        m_snaps->removePoint(timelinePos);
        if (proposed_size > 0) {
            // only test move if proposed_size is valid
            bool success = false;
            if (isClip(itemId)) {
                success = m_allClips[itemId]->requestResize(proposed_size, right, temp_undo, temp_redo, false);
            } else {
                success = m_allCompositions[itemId]->requestResize(proposed_size, right, temp_undo, temp_redo, false);
            }
            if (success) {
                temp_undo(); // undo temp move
                size = proposed_size;
            }
        }
    }
    return size;
}

int TimelineModel::requestItemSpeedChange(int itemId, int size, bool right, int snapDistance)
{
    Q_ASSERT(isClip(itemId));
    QWriteLocker locker(&m_lock);
    TRACE(itemId, size, right, snapDistance);
    Q_ASSERT(isItem(itemId));
    if (size <= 0) {
        TRACE_RES(-1);
        return -1;
    }
    int in = getItemPosition(itemId);
    int out = in + getItemPlaytime(itemId);

    if (right && size > out - in) {
        int targetPos = in + size - 1;
        int trackId = getItemTrackId(itemId);
        if (!getTrackById_const(trackId)->isBlankAt(targetPos) || !getItemsInRange(trackId, out + 1, targetPos, false).empty()) {
            size = getTrackById_const(trackId)->getBlankEnd(out + 1) - in;
        }
    } else if (!right && size > (out - in)) {
        int targetPos = out - size;
        int trackId = getItemTrackId(itemId);
        if (!getTrackById_const(trackId)->isBlankAt(targetPos) || !getItemsInRange(trackId, targetPos, in - 1, false).empty()) {
            size = out - getTrackById_const(trackId)->getBlankStart(in - 1);
        }
    }
    int timelinePos = pCore->getTimelinePosition();
    m_snaps->addPoint(timelinePos);
    int proposed_size = m_snaps->proposeSize(in, out, getBoundaries(itemId), size, right, snapDistance);
    m_snaps->removePoint(timelinePos);
    qDebug()<<"==== RESIZE REQUEST: "<<size<<"*, RESULKT: "<<proposed_size;
    return proposed_size > 0 ? proposed_size : size;
}


int TimelineModel::requestItemResize(int itemId, int size, bool right, bool logUndo, int snapDistance, bool allowSingleResize)
{
    if (logUndo) {
        qDebug() << "---------------------\n---------------------\nRESIZE W/UNDO CALLED\n++++++++++++++++\n++++";
    }
    QWriteLocker locker(&m_lock);
    TRACE(itemId, size, right, logUndo, snapDistance, allowSingleResize)
    Q_ASSERT(isItem(itemId));
    if (size <= 0) {
        TRACE_RES(-1)
        return -1;
    }
    int in = getItemPosition(itemId);
    int out = in + getItemPlaytime(itemId);
    size = requestItemResizeInfo(itemId, in, out, size, right, snapDistance);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::unordered_set<int> all_items;
    all_items.insert(itemId);
    if (!allowSingleResize && m_groups->isInGroup(itemId)) {
        int groupId = m_groups->getRootId(itemId);
        std::unordered_set<int> items = m_groups->getLeaves(groupId);
        /*if (m_groups->getType(groupId) == GroupType::AVSplit) {
            // Only resize group elements if it is an avsplit
            items = m_groups->getLeaves(groupId);
        }*/
        for (int id : items) {
            if (id == itemId) {
                continue;
            }
            int start = getItemPosition(id);
            int end = start + getItemPlaytime(id);
            if (right) {
                if (out == end) {
                    all_items.insert(id);
                }
            } else if (start == in) {
                all_items.insert(id);
            }
        }
    }
    bool result = true;
    int finalPos = right ? in + size : out - size;
    int finalSize;
    int resizedCount = 0;
    for (int id : all_items) {
        int tid = getItemTrackId(id);
        if (tid > -1 && getTrackById_const(tid)->isLocked()) {
            continue;
        }
        if (right) {
            finalSize = finalPos - getItemPosition(id);
        } else {
            finalSize = getItemPosition(id) + getItemPlaytime(id) - finalPos;
        }
        result = result && requestItemResize(id, finalSize, right, logUndo, undo, redo);
        resizedCount++;
    }
    if (!result || resizedCount == 0) {
        bool undone = undo();
        Q_ASSERT(undone);
        TRACE_RES(-1)
        return -1;
    }
    if (result && logUndo) {
        if (isClip(itemId)) {
            PUSH_UNDO(undo, redo, i18n("Resize clip"))
        } else {
            PUSH_UNDO(undo, redo, i18n("Resize composition"))
        }
    }
    int res = result ? size : -1;
    TRACE_RES(res)
    return res;
}

bool TimelineModel::requestItemResize(int itemId, int size, bool right, bool logUndo, Fun &undo, Fun &redo, bool blockUndo)
{
    Q_UNUSED(blockUndo)
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    bool result = false;
    if (isClip(itemId)) {
        result = m_allClips[itemId]->requestResize(size, right, local_undo, local_redo, logUndo);
    } else {
        Q_ASSERT(isComposition(itemId));
        result = m_allCompositions[itemId]->requestResize(size, right, local_undo, local_redo, logUndo);
    }
    if (result) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    return result;
}

int TimelineModel::requestClipsGroup(const std::unordered_set<int> &ids, bool logUndo, GroupType type)
{
    QWriteLocker locker(&m_lock);
    TRACE(ids, logUndo, type);
    if (type == GroupType::Selection || type == GroupType::Leaf) {
        // Selections shouldn't be done here. Call requestSetSelection instead
        TRACE_RES(-1);
        return -1;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    int result = requestClipsGroup(ids, undo, redo, type);
    if (result > -1 && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Group clips"));
    }
    TRACE_RES(result);
    return result;
}

int TimelineModel::requestClipsGroup(const std::unordered_set<int> &ids, Fun &undo, Fun &redo, GroupType type)
{
    QWriteLocker locker(&m_lock);
    if (type != GroupType::Selection) {
        requestClearSelection();
    }
    int clipsCount = 0;
    QList<int> tracks;
    for (int id : ids) {
        if (isClip(id)) {
            int trackId = getClipTrackId(id);
            if (trackId == -1) {
                return -1;
            }
            tracks << trackId;
            clipsCount++;
        } else if (isComposition(id)) {
            if (getCompositionTrackId(id) == -1) {
                return -1;
            }
        } else if (!isGroup(id)) {
            return -1;
        }
    }
    if (type == GroupType::Selection && ids.size() == 1) {
        // only one element selected, no group created
        return -1;
    }
    if (ids.size() == 2 && clipsCount == 2 && type == GroupType::Normal) {
        // Check if we are grouping an AVSplit
        std::unordered_set<int>::const_iterator it = ids.begin();
        int firstId = *it;
        std::advance(it, 1);
        int secondId = *it;
        bool isAVGroup = false;
        if (getClipBinId(firstId) == getClipBinId(secondId)) {
            if (getClipState(firstId) == PlaylistState::AudioOnly) {
                if (getClipState(secondId) == PlaylistState::VideoOnly) {
                    isAVGroup = true;
                }
            } else if (getClipState(secondId) == PlaylistState::AudioOnly) {
                isAVGroup = true;
            }
        }
        if (isAVGroup) {
            type = GroupType::AVSplit;
        }
    }
    int groupId = m_groups->groupItems(ids, undo, redo, type);
    if (type != GroupType::Selection) {
        // we make sure that the undo and the redo are going to unselect before doing anything else
        Fun unselect = [this]() { return requestClearSelection(); };
        PUSH_FRONT_LAMBDA(unselect, undo);
        PUSH_FRONT_LAMBDA(unselect, redo);
    }
    return groupId;
}

bool TimelineModel::requestClipsUngroup(const std::unordered_set<int> &itemIds, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemIds, logUndo);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = true;
    requestClearSelection();
    std::unordered_set<int> roots;
    std::transform(itemIds.begin(), itemIds.end(), std::inserter(roots, roots.begin()), [&](int id) { return m_groups->getRootId(id); });
    for (int root : roots) {
        if (isGroup(root)) {
            result = result && requestClipUngroup(root, undo, redo);
        }
    }
    if (!result) {
        bool undone = undo();
        Q_ASSERT(undone);
    }
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Ungroup clips"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestClipUngroup(int itemId, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, logUndo);
    requestClearSelection();
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = true;
    result = requestClipUngroup(itemId, undo, redo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Ungroup clips"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestClipUngroup(int itemId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    bool isSelection = m_groups->getType(m_groups->getRootId(itemId)) == GroupType::Selection;
    if (!isSelection) {
        requestClearSelection();
    }
    bool res = m_groups->ungroupItem(itemId, undo, redo);
    if (res && !isSelection) {
        // we make sure that the undo and the redo are going to unselect before doing anything else
        Fun unselect = [this]() { return requestClearSelection(); };
        PUSH_FRONT_LAMBDA(unselect, undo);
        PUSH_FRONT_LAMBDA(unselect, redo);
    }
    return res;
}

bool TimelineModel::requestTrackInsertion(int position, int &id, const QString &trackName, bool audioTrack)
{
    QWriteLocker locker(&m_lock);
    TRACE(position, id, trackName, audioTrack);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestTrackInsertion(position, id, trackName, audioTrack, undo, redo);
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Insert Track"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestTrackInsertion(int position, int &id, const QString &trackName, bool audioTrack, Fun &undo, Fun &redo, bool addCompositing)
{
    // TODO: make sure we disable overlayTrack before inserting a track
    qDebug()<<"=== REQUESTING TRACK INSERTION AT: "<<position;
    if (position == -1) {
        position = (int)(m_allTracks.size());
    }
    if (position < 0 || position > (int)m_allTracks.size()) {
        return false;
    }
    int previousId = -1;
    if (position < (int)m_allTracks.size()) {
        previousId = getTrackIndexFromPosition(position);
    }
    int trackId = TimelineModel::getNextId();
    id = trackId;
    Fun local_undo = deregisterTrack_lambda(trackId);
    TrackModel::construct(shared_from_this(), trackId, position, trackName, audioTrack);
    // Adjust compositions that were affecting track at previous pos
    QList <std::shared_ptr<CompositionModel>> updatedCompositions;
    if (previousId > -1) {
        for (auto &compo : m_allCompositions) {
            if (position > 0 && compo.second->getATrack() == position && compo.second->getForcedTrack() == -1) {
                updatedCompositions << compo.second;
            }
        }
    }
    Fun local_update = [position, updatedCompositions]() {
        for (auto &compo : updatedCompositions) {
            compo->setATrack(position + 1, -1);
        }
        return true;
    };
    Fun local_update_undo = [position, updatedCompositions]() {
        for (auto &compo : updatedCompositions) {
            compo->setATrack(position, -1);
        }
        return true;
    };

    Fun local_name_update = [position, audioTrack, this]() {
        if (audioTrack) {
            for (int i = 0; i <= position; i++) {
                QModelIndex ix = makeTrackIndexFromID(getTrackIndexFromPosition(i));
                emit dataChanged(ix, ix, {TimelineModel::TrackTagRole});
            }
        } else {
            for (int i = position; i < (int)m_allTracks.size(); i++) {
                QModelIndex ix = makeTrackIndexFromID(getTrackIndexFromPosition(i));
                emit dataChanged(ix, ix, {TimelineModel::TrackTagRole});
            }
        }
        return true;
    };

    local_update();
    local_name_update();
    Fun rebuild_compositing = [this]() {
        buildTrackCompositing(true);
        return true;
    };
    if (addCompositing) {
        buildTrackCompositing(true);
    }
    auto track = getTrackById(trackId);
    Fun local_redo = [track, position, local_update, addCompositing, this]() {
        // We capture a shared_ptr to the track, which means that as long as this undo object lives, the track object is not deleted. To insert it back it is
        // sufficient to register it.
        registerTrack(track, position, true);
        local_update();
        if (addCompositing) {
            buildTrackCompositing(true);
        }
        return true;
    };
    if (addCompositing) {
        PUSH_LAMBDA(local_update_undo, local_undo);
        PUSH_LAMBDA(rebuild_compositing, local_undo);
    }
    PUSH_LAMBDA(local_name_update, local_undo);
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    PUSH_LAMBDA(local_name_update, redo);
    return true;
}

bool TimelineModel::requestTrackDeletion(int trackId)
{
    // TODO: make sure we disable overlayTrack before deleting a track
    QWriteLocker locker(&m_lock);
    TRACE(trackId);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestTrackDeletion(trackId, undo, redo);
    if (result) {
        if (m_videoTarget == trackId) {
            m_videoTarget = -1;
        }
        if (m_audioTarget.contains(trackId)) {
            m_audioTarget.remove(trackId);
        }
        PUSH_UNDO(undo, redo, i18n("Delete Track"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestTrackDeletion(int trackId, Fun &undo, Fun &redo)
{
    Q_ASSERT(isTrack(trackId));
    if (m_allTracks.size() < 2) {
        pCore->displayMessage(i18n("Cannot delete last track in timeline"), InformationMessage, 500);
        return false;
    }
    std::vector<int> clips_to_delete;
    for (const auto &it : getTrackById(trackId)->m_allClips) {
        clips_to_delete.push_back(it.first);
    }
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    for (int clip : clips_to_delete) {
        bool res = true;
        while (res && m_groups->isInGroup(clip)) {
            res = requestClipUngroup(clip, local_undo, local_redo);
        }
        if (res) {
            res = requestClipDeletion(clip, local_undo, local_redo);
        }
        if (!res) {
            bool u = local_undo();
            Q_ASSERT(u);
            return false;
        }
    }
    std::vector<int> compositions_to_delete;
    for (const auto &it : getTrackById(trackId)->m_allCompositions) {
        compositions_to_delete.push_back(it.first);
    }
    for (int compo : compositions_to_delete) {
        bool res = true;
        while (res && m_groups->isInGroup(compo)) {
            res = requestClipUngroup(compo, local_undo, local_redo);
        }
        if (res) {
            res = requestCompositionDeletion(compo, local_undo, local_redo);
        }
        if (!res) {
            bool u = local_undo();
            Q_ASSERT(u);
            return false;
        }
    }
    int old_position = getTrackPosition(trackId);
    int previousTrack = getPreviousVideoTrackPos(trackId);
    auto operation = deregisterTrack_lambda(trackId);
    std::shared_ptr<TrackModel> track = getTrackById(trackId);
    bool audioTrack = track->isAudioTrack();
    QList <std::shared_ptr<CompositionModel>> updatedCompositions;
    for (auto &compo : m_allCompositions) {
        if (compo.second->getATrack() == old_position + 1 && compo.second->getForcedTrack() == -1) {
            updatedCompositions << compo.second;
        }
    }
    Fun reverse = [this, track, old_position, updatedCompositions]() {
        // We capture a shared_ptr to the track, which means that as long as this undo object lives, the track object is not deleted. To insert it back it is
        // sufficient to register it.
        registerTrack(track, old_position);
        for (auto &compo : updatedCompositions) {
            compo->setATrack(old_position + 1, -1);
        }
        return true;
    };
    Fun local_update = [previousTrack, updatedCompositions]() {
        for (auto &compo : updatedCompositions) {
            compo->setATrack(previousTrack, -1);
        }
        return true;
    };
    Fun rebuild_compositing = [this]() {
        buildTrackCompositing(true);
        return true;
    };
    Fun local_name_update = [old_position, audioTrack, this]() {
        if (audioTrack) {
            for (int i = 0; i < qMin(old_position + 1, getTracksCount()); i++) {
                QModelIndex ix = makeTrackIndexFromID(getTrackIndexFromPosition(i));
                emit dataChanged(ix, ix, {TimelineModel::TrackTagRole});
            }
        } else {
            for (int i = old_position; i < getTracksCount(); i++) {
                QModelIndex ix = makeTrackIndexFromID(getTrackIndexFromPosition(i));
                emit dataChanged(ix, ix, {TimelineModel::TrackTagRole});
            }
        }
        return true;
    };
    if (operation()) {
        local_update();
        rebuild_compositing();
        local_name_update();
        PUSH_LAMBDA(rebuild_compositing, local_undo);
        PUSH_LAMBDA(local_name_update, local_undo);
        UPDATE_UNDO_REDO(operation, reverse, local_undo, local_redo);
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        PUSH_LAMBDA(local_update, redo);
        PUSH_LAMBDA(rebuild_compositing, redo);
        PUSH_LAMBDA(local_name_update, redo);
        return true;
    }
    local_undo();
    return false;
}

void TimelineModel::registerTrack(std::shared_ptr<TrackModel> track, int pos, bool doInsert)
{
    // qDebug() << "REGISTER TRACK" << track->getId() << pos;
    int id = track->getId();
    if (pos == -1) {
        pos = static_cast<int>(m_allTracks.size());
    }
    Q_ASSERT(pos >= 0);
    Q_ASSERT(pos <= static_cast<int>(m_allTracks.size()));

    // effective insertion (MLT operation), add 1 to account for black background track
    if (doInsert) {
        int error = m_tractor->insert_track(*track, pos + 1);
        Q_ASSERT(error == 0); // we might need better error handling...
    }

    // we now insert in the list
    auto posIt = m_allTracks.begin();
    std::advance(posIt, pos);
    beginInsertRows(QModelIndex(), pos, pos);
    auto it = m_allTracks.insert(posIt, std::move(track));
    // it now contains the iterator to the inserted element, we store it
    Q_ASSERT(m_iteratorTable.count(id) == 0); // check that id is not used (shouldn't happen)
    m_iteratorTable[id] = it;
    endInsertRows();
    int cache = (int)QThread::idealThreadCount() + ((int)m_allTracks.size() + 1) * 2;
    mlt_service_cache_set_size(NULL, "producer_avformat", qMax(4, cache));
}

void TimelineModel::registerClip(const std::shared_ptr<ClipModel> &clip, bool registerProducer)
{
    int id = clip->getId();
    qDebug() << " // /REQUEST TL CLP REGSTR: " << id << "\n--------\nCLIPS COUNT: " << m_allClips.size();
    Q_ASSERT(m_allClips.count(id) == 0);
    m_allClips[id] = clip;
    clip->registerClipToBin(clip->getProducer(), registerProducer);
    m_groups->createGroupItem(id);
    clip->setTimelineEffectsEnabled(m_timelineEffectsEnabled);
}

void TimelineModel::registerGroup(int groupId)
{
    Q_ASSERT(m_allGroups.count(groupId) == 0);
    m_allGroups.insert(groupId);
}

Fun TimelineModel::deregisterTrack_lambda(int id)
{
    return [this, id]() {
        // qDebug() << "DEREGISTER TRACK" << id;
        emit checkTrackDeletion(id);
        auto it = m_iteratorTable[id];                        // iterator to the element
        int index = getTrackPosition(id);                     // compute index in list

        // send update to the model
        beginRemoveRows(QModelIndex(), index, index);
        // melt operation, add 1 to account for black background track
        m_tractor->remove_track(static_cast<int>(index + 1));
        // actual deletion of object
        m_allTracks.erase(it);
        // clean table
        m_iteratorTable.erase(id);
        // Finish operation
        endRemoveRows();
        int cache = (int)QThread::idealThreadCount() + ((int)m_allTracks.size() + 1) * 2;
        mlt_service_cache_set_size(NULL, "producer_avformat", qMax(4, cache));
        return true;
    };
}

Fun TimelineModel::deregisterClip_lambda(int clipId)
{
    return [this, clipId]() {
        // qDebug() << " // /REQUEST TL CLP DELETION: " << clipId << "\n--------\nCLIPS COUNT: " << m_allClips.size();
        // Clear effect stack
        clearAssetView(clipId);
        emit checkItemDeletion(clipId);
        Q_ASSERT(m_allClips.count(clipId) > 0);
        Q_ASSERT(getClipTrackId(clipId) == -1); // clip must be deleted from its track at this point
        Q_ASSERT(!m_groups->isInGroup(clipId)); // clip must be ungrouped at this point
        auto clip = m_allClips[clipId];
        m_allClips.erase(clipId);
        clip->deregisterClipToBin();
        m_groups->destructGroupItem(clipId);
        return true;
    };
}

void TimelineModel::deregisterGroup(int id)
{
    Q_ASSERT(m_allGroups.count(id) > 0);
    m_allGroups.erase(id);
}

std::shared_ptr<TrackModel> TimelineModel::getTrackById(int trackId)
{
    Q_ASSERT(m_iteratorTable.count(trackId) > 0);
    return *m_iteratorTable[trackId];
}

const std::shared_ptr<TrackModel> TimelineModel::getTrackById_const(int trackId) const
{
    Q_ASSERT(m_iteratorTable.count(trackId) > 0);
    return *m_iteratorTable.at(trackId);
}

bool TimelineModel::addTrackEffect(int trackId, const QString &effectId)
{
    Q_ASSERT(m_iteratorTable.count(trackId) > 0);
    if ((*m_iteratorTable.at(trackId))->addEffect(effectId) == false) {
        QString effectName = EffectsRepository::get()->getName(effectId);
        pCore->displayMessage(i18n("Cannot add effect %1 to selected track", effectName), InformationMessage, 500);
        return false;
    }
    return true;
}

bool TimelineModel::copyTrackEffect(int trackId, const QString &sourceId)
{
    QStringList source = sourceId.split(QLatin1Char('-'));
    Q_ASSERT(m_iteratorTable.count(trackId) > 0 && source.count() == 3);
    int itemType = source.at(0).toInt();
    int itemId = source.at(1).toInt();
    int itemRow = source.at(2).toInt();
    std::shared_ptr<EffectStackModel> effectStack = pCore->getItemEffectStack(itemType, itemId);
    if ((*m_iteratorTable.at(trackId))->copyEffect(effectStack, itemRow) == false) {
        pCore->displayMessage(i18n("Cannot paste effect to selected track"), InformationMessage, 500);
        return false;
    }
    return true;
}

std::shared_ptr<ClipModel> TimelineModel::getClipPtr(int clipId) const
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId);
}

bool TimelineModel::addClipEffect(int clipId, const QString &effectId, bool notify)
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    bool result = m_allClips.at(clipId)->addEffect(effectId);
    if (!result && notify) {
        QString effectName = EffectsRepository::get()->getName(effectId);
        pCore->displayMessage(i18n("Cannot add effect %1 to selected clip", effectName), InformationMessage, 500);
    }
    return result;
}

bool TimelineModel::removeFade(int clipId, bool fromStart)
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId)->removeFade(fromStart);
}

std::shared_ptr<EffectStackModel> TimelineModel::getClipEffectStack(int itemId)
{
    Q_ASSERT(m_allClips.count(itemId));
    return m_allClips.at(itemId)->m_effectStack;
}

bool TimelineModel::copyClipEffect(int clipId, const QString &sourceId)
{
    QStringList source = sourceId.split(QLatin1Char('-'));
    Q_ASSERT(m_allClips.count(clipId) && source.count() == 3);
    int itemType = source.at(0).toInt();
    int itemId = source.at(1).toInt();
    int itemRow = source.at(2).toInt();
    std::shared_ptr<EffectStackModel> effectStack = pCore->getItemEffectStack(itemType, itemId);
    return m_allClips.at(clipId)->copyEffect(effectStack, itemRow);
}

bool TimelineModel::adjustEffectLength(int clipId, const QString &effectId, int duration, int initialDuration)
{
    Q_ASSERT(m_allClips.count(clipId));
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = m_allClips.at(clipId)->adjustEffectLength(effectId, duration, initialDuration, undo, redo);
    if (res && initialDuration > 0) {
        PUSH_UNDO(undo, redo, i18n("Adjust Fade"));
    }
    return res;
}

std::shared_ptr<CompositionModel> TimelineModel::getCompositionPtr(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    return m_allCompositions.at(compoId);
}

int TimelineModel::getNextId()
{
    return TimelineModel::next_id++;
}

bool TimelineModel::isClip(int id) const
{
    return m_allClips.count(id) > 0;
}

bool TimelineModel::isComposition(int id) const
{
    return m_allCompositions.count(id) > 0;
}

bool TimelineModel::isItem(int id) const
{
    return isClip(id) || isComposition(id);
}

bool TimelineModel::isTrack(int id) const
{
    return m_iteratorTable.count(id) > 0;
}

bool TimelineModel::isGroup(int id) const
{
    return m_allGroups.count(id) > 0;
}

void TimelineModel::updateDuration()
{
    if (m_closing) {
        return;
    }
    int current = m_blackClip->get_playtime() - TimelineModel::seekDuration;
    int duration = 0;
    for (const auto &tck : m_iteratorTable) {
        auto track = (*tck.second);
        duration = qMax(duration, track->trackDuration());
    }
    if (duration != current) {
        // update black track length
        m_blackClip->set("out", duration + TimelineModel::seekDuration);
        emit durationUpdated();
    }
}

int TimelineModel::duration() const
{
    return m_tractor->get_playtime() - TimelineModel::seekDuration;
}

std::unordered_set<int> TimelineModel::getGroupElements(int clipId)
{
    int groupId = m_groups->getRootId(clipId);
    return m_groups->getLeaves(groupId);
}

Mlt::Profile *TimelineModel::getProfile()
{
    return m_profile;
}

bool TimelineModel::requestReset(Fun &undo, Fun &redo)
{
    std::vector<int> all_ids;
    for (const auto &track : m_iteratorTable) {
        all_ids.push_back(track.first);
    }
    bool ok = true;
    for (int trackId : all_ids) {
        ok = ok && requestTrackDeletion(trackId, undo, redo);
    }
    return ok;
}

void TimelineModel::setUndoStack(std::weak_ptr<DocUndoStack> undo_stack)
{
    m_undoStack = std::move(undo_stack);
}

int TimelineModel::suggestSnapPoint(int pos, int snapDistance)
{
    int snapped = m_snaps->getClosestPoint(pos);
    return (qAbs(snapped - pos) < snapDistance ? snapped : pos);
}

int TimelineModel::getBestSnapPos(int referencePos, int diff, std::vector<int> pts, int cursorPosition, int snapDistance)
{
    if (!pts.empty()) {
        if (m_editMode == TimelineMode::NormalEdit) {
            m_snaps->ignore(pts);
        }
    } else {
        return -1;
    }
    // Sort and remove duplicates
    std::sort(pts.begin(), pts.end());
    pts.erase( std::unique(pts.begin(), pts.end()), pts.end());
    m_snaps->addPoint(cursorPosition);
    int closest = -1;
    int lowestDiff = snapDistance + 1;
    for (int point : pts) {
        int snapped = m_snaps->getClosestPoint(point + diff);
        int currentDiff = qAbs(point + diff - snapped);
        if (currentDiff < lowestDiff) {
            lowestDiff = currentDiff;
            closest = snapped - (point - referencePos);
            if (lowestDiff < 2) {
                break;
            }
        }
    }
    if (m_editMode == TimelineMode::NormalEdit) {
        m_snaps->unIgnore();
    }
    m_snaps->removePoint(cursorPosition);
    return closest;
}

int TimelineModel::getNextSnapPos(int pos, std::vector<int> &snaps)
{
    QVector<int>tracks;
    // Get active tracks
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        if ((*it)->shouldReceiveTimelineOp()) {
            tracks << (*it)->getId();
        }
        ++it;
    }
    if (tracks.isEmpty() || tracks.count() == (int) m_allTracks.size()) {
        // No active track, use all possible snap points
        return m_snaps->getNextPoint((int)pos);
    }
    // Build snap points for selected tracks
    for (const auto &cp : m_allClips) {
        // Check if clip is on a target track
        if (tracks.contains(cp.second->getCurrentTrackId())) {
            auto clip = (cp.second);
            clip->allSnaps(snaps);
        }
    }
    // sort snaps
    std::sort(snaps.begin(), snaps.end());
    for (auto i : snaps) {
        if ((int)i > pos) {
            return (int) i;
        }
    }
    return pos;
}

int TimelineModel::getPreviousSnapPos(int pos, std::vector<int> &snaps)
{
    QVector<int>tracks;
    // Get active tracks
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        if ((*it)->shouldReceiveTimelineOp()) {
            tracks << (*it)->getId();
        }
        ++it;
    }
    if (tracks.isEmpty() || tracks.count() == (int) m_allTracks.size()) {
        // No active track, use all possible snap points
        return m_snaps->getPreviousPoint((int)pos);
    }
    // Build snap points for selected tracks
    for (const auto &cp : m_allClips) {
        // Check if clip is on a target track
        if (tracks.contains(cp.second->getCurrentTrackId())) {
            auto clip = (cp.second);
            clip->allSnaps(snaps);
        }
    }
    // sort snaps
    std::sort(snaps.begin(), snaps.end());
    // sort descending
    std::reverse(snaps.begin(),snaps.end());
    for (auto i : snaps) {
        if ((int)i < pos) {
            return (int)i;
        }
    }
    return 0;
}

void TimelineModel::addSnap(int pos)
{
    TRACE(pos);
    return m_snaps->addPoint(pos);
}

void TimelineModel::removeSnap(int pos)
{
    TRACE(pos);
    return m_snaps->removePoint(pos);
}

void TimelineModel::registerComposition(const std::shared_ptr<CompositionModel> &composition)
{
    int id = composition->getId();
    Q_ASSERT(m_allCompositions.count(id) == 0);
    m_allCompositions[id] = composition;
    m_groups->createGroupItem(id);
}

bool TimelineModel::requestCompositionInsertion(const QString &transitionId, int trackId, int position, int length, std::unique_ptr<Mlt::Properties> transProps,
                                                int &id, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    // TRACE(transitionId, trackId, position, length, transProps.get(), id, logUndo);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestCompositionInsertion(transitionId, trackId, -1, position, length, std::move(transProps), id, undo, redo, logUndo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Insert Composition"));
    }
    // TRACE_RES(result);
    return result;
}

bool TimelineModel::requestCompositionInsertion(const QString &transitionId, int trackId, int compositionTrack, int position, int length,
                                                std::unique_ptr<Mlt::Properties> transProps, int &id, Fun &undo, Fun &redo, bool finalMove, QString originalDecimalPoint)
{
    qDebug() << "Inserting compo track" << trackId << "pos" << position << "length" << length;
    int compositionId = TimelineModel::getNextId();
    id = compositionId;
    Fun local_undo = deregisterComposition_lambda(compositionId);
    CompositionModel::construct(shared_from_this(), transitionId, originalDecimalPoint, compositionId, std::move(transProps));
    auto composition = m_allCompositions[compositionId];
    Fun local_redo = [composition, this]() {
        // We capture a shared_ptr to the composition, which means that as long as this undo object lives, the composition object is not deleted. To insert it
        // back it is sufficient to register it.
        registerComposition(composition);
        return true;
    };
    bool res = requestCompositionMove(compositionId, trackId, compositionTrack, position, true, finalMove, local_undo, local_redo);
    qDebug() << "trying to move" << trackId << "pos" << position << "success " << res;
    if (res) {
        res = requestItemResize(compositionId, length, true, true, local_undo, local_redo, true);
        qDebug() << "trying to resize" << compositionId << "length" << length << "success " << res;
    }
    if (!res) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        id = -1;
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

Fun TimelineModel::deregisterComposition_lambda(int compoId)
{
    return [this, compoId]() {
        Q_ASSERT(m_allCompositions.count(compoId) > 0);
        Q_ASSERT(!m_groups->isInGroup(compoId)); // composition must be ungrouped at this point
        requestClearSelection(true);
        clearAssetView(compoId);
        m_allCompositions.erase(compoId);
        m_groups->destructGroupItem(compoId);
        return true;
    };
}

int TimelineModel::getCompositionPosition(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    const auto trans = m_allCompositions.at(compoId);
    return trans->getPosition();
}

int TimelineModel::getCompositionPlaytime(int compoId) const
{
    READ_LOCK();
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    const auto trans = m_allCompositions.at(compoId);
    int playtime = trans->getPlaytime();
    return playtime;
}

int TimelineModel::getItemPosition(int itemId) const
{
    if (isClip(itemId)) {
        return getClipPosition(itemId);
    }
    return getCompositionPosition(itemId);
}

int TimelineModel::getItemPlaytime(int itemId) const
{
    if (isClip(itemId)) {
        return getClipPlaytime(itemId);
    }
    return getCompositionPlaytime(itemId);
}

int TimelineModel::getTrackCompositionsCount(int trackId) const
{
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->getCompositionsCount();
}

bool TimelineModel::requestCompositionMove(int compoId, int trackId, int position, bool updateView, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isComposition(compoId));
    if (m_allCompositions[compoId]->getPosition() == position && getCompositionTrackId(compoId) == trackId) {
        return true;
    }
    if (m_groups->isInGroup(compoId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(compoId);
        int current_trackId = getCompositionTrackId(compoId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allCompositions[compoId]->getPosition();
        return requestGroupMove(compoId, groupId, delta_track, delta_pos, true, updateView, logUndo);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int min = getCompositionPosition(compoId);
    int max = min + getCompositionPlaytime(compoId);
    int tk = getCompositionTrackId(compoId);
    bool res = requestCompositionMove(compoId, trackId, m_allCompositions[compoId]->getForcedTrack(), position, updateView, logUndo, undo, redo);
    if (tk > -1) {
        min = qMin(min, getCompositionPosition(compoId));
        max = qMax(max, getCompositionPosition(compoId));
    } else {
        min = getCompositionPosition(compoId);
        max = min + getCompositionPlaytime(compoId);
    }

    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move composition"));
        checkRefresh(min, max);
    }
    return res;
}

bool TimelineModel::isAudioTrack(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    return (*it)->isAudioTrack();
}

bool TimelineModel::requestCompositionMove(int compoId, int trackId, int compositionTrack, int position, bool updateView, bool finalMove, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isComposition(compoId));
    Q_ASSERT(isTrack(trackId));
    if (compositionTrack == -1 || (compositionTrack > 0 && trackId == getTrackIndexFromPosition(compositionTrack - 1))) {
        // qDebug() << "// compo track: " << trackId << ", PREVIOUS TK: " << getPreviousVideoTrackPos(trackId);
        compositionTrack = getPreviousVideoTrackPos(trackId);
    }
    if (compositionTrack == -1) {
        // it doesn't make sense to insert a composition on the last track
        qDebug() << "Move failed because of last track";
        return false;
    }
    qDebug() << "Requesting composition move" << trackId << "," << position << " ( " << compositionTrack << " / "
             << (compositionTrack > 0 ? getTrackIndexFromPosition(compositionTrack - 1) : 0);

    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    bool ok = true;
    int old_trackId = getCompositionTrackId(compoId);
    bool notifyViewOnly = false;
    Fun update_model = []() { return true; };
    if (updateView && old_trackId == trackId) {
        // Move on same track, only send view update
        updateView = false;
        notifyViewOnly = true;
        update_model = [compoId, this]() {
            QModelIndex modelIndex = makeCompositionIndexFromID(compoId);
            notifyChange(modelIndex, modelIndex, StartRole);
            return true;
        };
    }
    if (old_trackId != -1) {
        Fun delete_operation = []() { return true; };
        Fun delete_reverse = []() { return true; };
        if (old_trackId != trackId) {
            delete_operation = [this, compoId]() {
                bool res = unplantComposition(compoId);
                if (res) m_allCompositions[compoId]->setATrack(-1, -1);
                return res;
            };
            int oldAtrack = m_allCompositions[compoId]->getATrack();
            delete_reverse = [this, compoId, oldAtrack, updateView]() {
                m_allCompositions[compoId]->setATrack(oldAtrack, oldAtrack <= 0 ? -1 : getTrackIndexFromPosition(oldAtrack - 1));
                return replantCompositions(compoId, updateView);
            };
        }
        ok = delete_operation();
        if (!ok) qDebug() << "Move failed because of first delete operation";

        if (ok) {
            if (notifyViewOnly) {
                PUSH_LAMBDA(update_model, local_undo);
            }
            UPDATE_UNDO_REDO(delete_operation, delete_reverse, local_undo, local_redo);
            ok = getTrackById(old_trackId)->requestCompositionDeletion(compoId, updateView, finalMove, local_undo, local_redo, false);
        }
        if (!ok) {
            qDebug() << "Move failed because of first deletion request";
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    ok = getTrackById(trackId)->requestCompositionInsertion(compoId, position, updateView, finalMove, local_undo, local_redo);
    if (!ok) qDebug() << "Move failed because of second insertion request";
    if (ok) {
        Fun insert_operation = []() { return true; };
        Fun insert_reverse = []() { return true; };
        if (old_trackId != trackId) {
            insert_operation = [this, compoId, compositionTrack, updateView]() {
                qDebug() << "-------------- ATRACK ----------------\n" << compositionTrack << " = " << getTrackIndexFromPosition(compositionTrack);
                m_allCompositions[compoId]->setATrack(compositionTrack, compositionTrack <= 0 ? -1 : getTrackIndexFromPosition(compositionTrack - 1));
                return replantCompositions(compoId, updateView);
            };
            insert_reverse = [this, compoId]() {
                bool res = unplantComposition(compoId);
                if (res) m_allCompositions[compoId]->setATrack(-1, -1);
                return res;
            };
        }
        ok = insert_operation();
        if (!ok) qDebug() << "Move failed because of second insert operation";
        if (ok) {
            if (notifyViewOnly) {
                PUSH_LAMBDA(update_model, local_redo);
            }
            UPDATE_UNDO_REDO(insert_operation, insert_reverse, local_undo, local_redo);
        }
    }
    if (!ok) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    update_model();
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::replantCompositions(int currentCompo, bool updateView)
{
    // We ensure that the compositions are planted in a decreasing order of a_track, and increasing order of b_track.
    // For that, there is no better option than to disconnect every composition and then reinsert everything in the correct order.
    std::vector<std::pair<int, int>> compos;
    for (const auto &compo : m_allCompositions) {
        int trackId = compo.second->getCurrentTrackId();
        if (trackId == -1 || compo.second->getATrack() == -1) {
            continue;
        }
        // Note: we need to retrieve the position of the track, that is its melt index.
        int trackPos = getTrackMltIndex(trackId);
        compos.emplace_back(trackPos, compo.first);
        if (compo.first != currentCompo) {
            unplantComposition(compo.first);
        }
    }
    // sort by decreasing b_track
    std::sort(compos.begin(), compos.end(), [&](const std::pair<int, int> &a, const std::pair<int, int> &b) { 
        if (m_allCompositions[a.second]->getATrack() == m_allCompositions[b.second]->getATrack()) {
            return a.first < b.first;
        }
        return m_allCompositions[a.second]->getATrack() > m_allCompositions[b.second]->getATrack();
    });
    // replant
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();

    // Unplant track compositing
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString resource = mlt_properties_get(properties, "mlt_service");

    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    QList<Mlt::Transition *> trackCompositions;
    while (mlt_type == transition_type) {
        Mlt::Transition transition((mlt_transition)nextservice);
        nextservice = mlt_service_producer(nextservice);
        int internal = transition.get_int("internal_added");
        if (internal > 0 && resource != QLatin1String("mix")) {
            trackCompositions << new Mlt::Transition(transition);
            field->disconnect_service(transition);
            transition.disconnect_all_producers();
        }
        if (nextservice == nullptr) {
            break;
        }
        mlt_type = mlt_service_identify(nextservice);
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        resource = mlt_properties_get(properties, "mlt_service");
    }
    // Sort track compositing
    std::sort(trackCompositions.begin(), trackCompositions.end(), [](Mlt::Transition *a, Mlt::Transition *b) { return a->get_b_track() < b->get_b_track(); });

    for (const auto &compo : compos) {
        int aTrack = m_allCompositions[compo.second]->getATrack();
        Q_ASSERT(aTrack != -1 && aTrack < m_tractor->count());

        Mlt::Transition &transition = *m_allCompositions[compo.second].get();
        transition.set_tracks(aTrack, compo.first);
        int ret = field->plant_transition(transition, aTrack, compo.first);
        /*qDebug() << "Planting composition " << compo.second << "in " << aTrack << "/" << compo.first << "IN = " << m_allCompositions[compo.second]->getIn()
                 << "OUT = " << m_allCompositions[compo.second]->getOut() << "ret=" << ret;*/

        mlt_service consumer = mlt_service_consumer(transition.get_service());
        Q_ASSERT(consumer != nullptr);
        if (ret != 0) {
            field->unlock();
            return false;
        }
    }
    // Replant last tracks compositing
    while (!trackCompositions.isEmpty()) {
        Mlt::Transition *firstTr = trackCompositions.takeFirst();
        field->plant_transition(*firstTr, firstTr->get_a_track(), firstTr->get_b_track());
    }
    field->unlock();
    if (updateView) {
        QModelIndex modelIndex = makeCompositionIndexFromID(currentCompo);
        notifyChange(modelIndex, modelIndex, ItemATrack);
    }
    return true;
}

bool TimelineModel::unplantComposition(int compoId)
{
    //qDebug() << "Unplanting" << compoId;
    Mlt::Transition &transition = *m_allCompositions[compoId].get();
    mlt_service consumer = mlt_service_consumer(transition.get_service());
    Q_ASSERT(consumer != nullptr);
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    field->disconnect_service(transition);
    int ret = transition.disconnect_all_producers();

    mlt_service nextservice = mlt_service_get_producer(transition.get_service());
    // mlt_service consumer = mlt_service_consumer(transition.get_service());
    Q_ASSERT(nextservice == nullptr);
    // Q_ASSERT(consumer == nullptr);
    field->unlock();
    return ret != 0;
}

bool TimelineModel::checkConsistency()
{
    for (const auto &tck : m_iteratorTable) {
        auto track = (*tck.second);
        // Check parent/children link for tracks
        if (auto ptr = track->m_parent.lock()) {
            if (ptr.get() != this) {
                qDebug() << "Wrong parent for track" << tck.first;
                return false;
            }
        } else {
            qDebug() << "NULL parent for track" << tck.first;
            return false;
        }
        // check consistency of track
        if (!track->checkConsistency()) {
            qDebug() << "Consistency check failed for track" << tck.first;
            return false;
        }
    }

    // We store all in/outs of clips to check snap points
    std::map<int, int> snaps;
    // Check parent/children link for clips
    for (const auto &cp : m_allClips) {
        auto clip = (cp.second);
        // Check parent/children link for tracks
        if (auto ptr = clip->m_parent.lock()) {
            if (ptr.get() != this) {
                qDebug() << "Wrong parent for clip" << cp.first;
                return false;
            }
        } else {
            qDebug() << "NULL parent for clip" << cp.first;
            return false;
        }
        if (getClipTrackId(cp.first) != -1) {
            snaps[clip->getPosition()] += 1;
            snaps[clip->getPosition() + clip->getPlaytime()] += 1;
        }
        if (!clip->checkConsistency()) {
            qDebug() << "Consistency check failed for clip" << cp.first;
            return false;
        }
    }
    for (const auto &cp : m_allCompositions) {
        auto clip = (cp.second);
        // Check parent/children link for tracks
        if (auto ptr = clip->m_parent.lock()) {
            if (ptr.get() != this) {
                qDebug() << "Wrong parent for compo" << cp.first;
                return false;
            }
        } else {
            qDebug() << "NULL parent for compo" << cp.first;
            return false;
        }
        if (getCompositionTrackId(cp.first) != -1) {
            snaps[clip->getPosition()] += 1;
            snaps[clip->getPosition() + clip->getPlaytime()] += 1;
        }
    }
    // Check snaps
    auto stored_snaps = m_snaps->_snaps();
    if (snaps.size() != stored_snaps.size()) {
        qDebug() << "Wrong number of snaps: " << snaps.size() << " == " << stored_snaps.size();
        return false;
    }
    for (auto i = snaps.begin(), j = stored_snaps.begin(); i != snaps.end(); ++i, ++j) {
        if (*i != *j) {
            qDebug() << "Wrong snap info at point" << (*i).first;
            return false;
        }
    }

    // We check consistency with bin model
    auto binClips = pCore->projectItemModel()->getAllClipIds();
    // First step: all clips referenced by the bin model exist and are inserted
    for (const auto &binClip : binClips) {
        auto projClip = pCore->projectItemModel()->getClipByBinID(binClip);
        for (const auto &insertedClip : projClip->m_registeredClips) {
            if (auto ptr = insertedClip.second.lock()) {
                if (ptr.get() == this) { // check we are talking of this timeline
                    if (!isClip(insertedClip.first)) {
                        qDebug() << "Bin model registers a bad clip ID" << insertedClip.first;
                        return false;
                    }
                }
            } else {
                qDebug() << "Bin model registers a clip in a NULL timeline" << insertedClip.first;
                return false;
            }
        }
    }

    // Second step: all clips are referenced
    for (const auto &clip : m_allClips) {
        auto binId = clip.second->m_binClipId;
        auto projClip = pCore->projectItemModel()->getClipByBinID(binId);
        if (projClip->m_registeredClips.count(clip.first) == 0) {
            qDebug() << "Clip " << clip.first << "not registered in bin";
            return false;
        }
    }

    // We now check consistency of the compositions. For that, we list all compositions of the tractor, and see if we have a matching one in our
    // m_allCompositions
    std::unordered_set<int> remaining_compo;
    for (const auto &compo : m_allCompositions) {
        if (getCompositionTrackId(compo.first) != -1 && m_allCompositions[compo.first]->getATrack() != -1) {
            remaining_compo.insert(compo.first);

            // check validity of the consumer
            Mlt::Transition &transition = *m_allCompositions[compo.first].get();
            mlt_service consumer = mlt_service_consumer(transition.get_service());
            Q_ASSERT(consumer != nullptr);
        }
    }
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();

    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    while (nextservice != nullptr) {
        if (mlt_type == transition_type) {
            auto tr = (mlt_transition)nextservice;
            if (mlt_properties_get_int( MLT_TRANSITION_PROPERTIES(tr), "internal_added") > 0) {
                // Skip track compositing
                nextservice = mlt_service_producer(nextservice);
                continue;
            }
            int currentTrack = mlt_transition_get_b_track(tr);
            int currentATrack = mlt_transition_get_a_track(tr);
            if (currentTrack == currentATrack) {
                // Skip invalid transitions created by MLT on track deletion
                nextservice = mlt_service_producer(nextservice);
                continue;
            }

            int currentIn = (int)mlt_transition_get_in(tr);
            int currentOut = (int)mlt_transition_get_out(tr);

            qDebug() << "looking composition IN: " << currentIn << ", OUT: " << currentOut << ", TRACK: " << currentTrack << " / " << currentATrack;
            int foundId = -1;
            // we iterate to try to find a matching compo
            for (int compoId : remaining_compo) {
                if (getTrackMltIndex(getCompositionTrackId(compoId)) == currentTrack && m_allCompositions[compoId]->getATrack() == currentATrack &&
                    m_allCompositions[compoId]->getIn() == currentIn && m_allCompositions[compoId]->getOut() == currentOut) {
                    foundId = compoId;
                    break;
                }
            }
            if (foundId == -1) {
                qDebug() << "Error, we didn't find matching composition IN: " << currentIn << ", OUT: " << currentOut << ", TRACK: " << currentTrack << " / "
                         << currentATrack <<", SERVICE: "<<mlt_properties_get( MLT_TRANSITION_PROPERTIES(tr), "mlt_service")<<"\nID: "<<mlt_properties_get( MLT_TRANSITION_PROPERTIES(tr), "id");
                field->unlock();
                return false;
            }
            qDebug() << "Found";

            remaining_compo.erase(foundId);
        }
        nextservice = mlt_service_producer(nextservice);
        if (nextservice == nullptr) {
            break;
        }
        mlt_type = mlt_service_identify(nextservice);
    }
    field->unlock();

    if (!remaining_compo.empty()) {
        qDebug() << "Error: We found less compositions than expected. Compositions that have not been found:";
        for (int compoId : remaining_compo) {
            qDebug() << compoId;
        }
        return false;
    }

    // We check consistency of groups
    if (!m_groups->checkConsistency(true, true)) {
        qDebug() << "== ERROR IN GROUP CONSISTENCY";
        return false;
    }

    // Check that the selection is in a valid state:
    if (m_currentSelection != -1 && !isClip(m_currentSelection) && !isComposition(m_currentSelection) && !isGroup(m_currentSelection)) {
        qDebug() << "Selection is in inconsistent state";
        return false;
    }
    return true;
}

void TimelineModel::setTimelineEffectsEnabled(bool enabled)
{
    m_timelineEffectsEnabled = enabled;
    // propagate info to clips
    for (const auto &clip : m_allClips) {
        clip.second->setTimelineEffectsEnabled(enabled);
    }

    // TODO if we support track effects, they should be disabled here too
}

std::shared_ptr<Mlt::Producer> TimelineModel::producer()
{
    return std::make_shared<Mlt::Producer>(tractor());
}

void TimelineModel::checkRefresh(int start, int end)
{
    if (m_blockRefresh) {
        return;
    }
    int currentPos = tractor()->position();
    if (currentPos >= start && currentPos < end) {
        emit requestMonitorRefresh();
    }
}

void TimelineModel::clearAssetView(int itemId)
{
    emit requestClearAssetView(itemId);
}

std::shared_ptr<AssetParameterModel> TimelineModel::getCompositionParameterModel(int compoId) const
{
    READ_LOCK();
    Q_ASSERT(isComposition(compoId));
    return std::static_pointer_cast<AssetParameterModel>(m_allCompositions.at(compoId));
}

std::shared_ptr<EffectStackModel> TimelineModel::getClipEffectStackModel(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(clipId));
    return std::static_pointer_cast<EffectStackModel>(m_allClips.at(clipId)->m_effectStack);
}

std::shared_ptr<EffectStackModel> TimelineModel::getTrackEffectStackModel(int trackId)
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    return getTrackById(trackId)->m_effectStack;
}

std::shared_ptr<EffectStackModel> TimelineModel::getMasterEffectStackModel()
{
    READ_LOCK();
    if (m_masterStack == nullptr) {
        m_masterService.reset(new Mlt::Service(*m_tractor.get()));
        m_masterStack = EffectStackModel::construct(m_masterService, {ObjectType::Master, 0}, m_undoStack);
    }
    return m_masterStack;
}

void TimelineModel::importMasterEffects(std::weak_ptr<Mlt::Service> service)
{
    READ_LOCK();
    if (m_masterStack == nullptr) {
        getMasterEffectStackModel();
    }
    m_masterStack->importEffects(std::move(service), PlaylistState::Disabled);
}


QStringList TimelineModel::extractCompositionLumas() const
{
    QStringList urls;
    for (const auto &compo : m_allCompositions) {
        QString luma = compo.second->getProperty(QStringLiteral("resource"));
        if (!luma.isEmpty()) {
            urls << QUrl::fromLocalFile(luma).toLocalFile();
        }
    }
    urls.removeDuplicates();
    return urls;
}

void TimelineModel::adjustAssetRange(int clipId, int in, int out)
{
    Q_UNUSED(clipId)
    Q_UNUSED(in)
    Q_UNUSED(out)
    // pCore->adjustAssetRange(clipId, in, out);
}

void TimelineModel::requestClipReload(int clipId)
{
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };

    // in order to make the producer change effective, we need to unplant / replant the clip in int track
    int old_trackId = getClipTrackId(clipId);
    int oldPos = getClipPosition(clipId);
    int oldOut = getClipIn(clipId) + getClipPlaytime(clipId);
    int maxDuration = m_allClips[clipId]->getMaxDuration();
    bool hasPitch = false;
    double speed = m_allClips[clipId]->getSpeed();
    PlaylistState::ClipState state = m_allClips[clipId]->clipState();
    if (!qFuzzyCompare(speed, 1.)) {
        hasPitch = m_allClips[clipId]->getIntProperty(QStringLiteral("warp_pitch"));
    }
    int audioStream = m_allClips[clipId]->getIntProperty(QStringLiteral("audio_index"));
    // Check if clip out is longer than actual producer duration (if user forced duration)
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(getClipBinId(clipId));
    bool refreshView = oldOut > (int)binClip->frameDuration();
    if (old_trackId != -1) {
        getTrackById(old_trackId)->requestClipDeletion(clipId, refreshView, true, local_undo, local_redo, false, false);
    }
    if (old_trackId != -1) {
        m_allClips[clipId]->refreshProducerFromBin(old_trackId, state, audioStream, 0, hasPitch);
        getTrackById(old_trackId)->requestClipInsertion(clipId, oldPos, refreshView, true, local_undo, local_redo);
        if (maxDuration != m_allClips[clipId]->getMaxDuration()) {
            QModelIndex ix = makeClipIndexFromID(clipId);
            emit dataChanged(ix, ix, {TimelineModel::MaxDurationRole});
        }
    }
}

void TimelineModel::replugClip(int clipId)
{
    int old_trackId = getClipTrackId(clipId);
    if (old_trackId != -1) {
        getTrackById(old_trackId)->replugClip(clipId);
    }
}

void TimelineModel::requestClipUpdate(int clipId, const QVector<int> &roles)
{
    QModelIndex modelIndex = makeClipIndexFromID(clipId);
    if (roles.contains(TimelineModel::ReloadThumbRole)) {
        m_allClips[clipId]->forceThumbReload = !m_allClips[clipId]->forceThumbReload;
    }
    notifyChange(modelIndex, modelIndex, roles);
}

bool TimelineModel::requestClipTimeWarp(int clipId, double speed, bool pitchCompensate, bool changeDuration, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    int oldPos = getClipPosition(clipId);
    // in order to make the producer change effective, we need to unplant / replant the clip in int track
    bool success = true;
    int trackId = getClipTrackId(clipId);
    if (trackId != -1) {
        success = success && getTrackById(trackId)->requestClipDeletion(clipId, true, true, local_undo, local_redo, false, false);
    }
    if (success) {
        success = m_allClips[clipId]->useTimewarpProducer(speed, pitchCompensate, changeDuration, local_undo, local_redo);
    }
    if (trackId != -1) {
        success = success && getTrackById(trackId)->requestClipInsertion(clipId, oldPos, true, true, local_undo, local_redo);
    }
    if (!success) {
        local_undo();
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return success;
}

bool TimelineModel::requestClipTimeWarp(int clipId, double speed, bool pitchCompensate, bool changeDuration)
{
    QWriteLocker locker(&m_lock);
    if (qFuzzyCompare(speed, m_allClips[clipId]->getSpeed()) && pitchCompensate == m_allClips[clipId]->getIntProperty("warp_pitch")) {
        return true;
    }
    TRACE(clipId, speed);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // Get main clip info
    int trackId = getClipTrackId(clipId);
    bool result = true;
    if (trackId != -1) {
        // Check if clip has a split partner
        int splitId = m_groups->getSplitPartner(clipId);
        if (splitId > -1) {
            result = requestClipTimeWarp(splitId, speed / 100.0, pitchCompensate, changeDuration, undo, redo);
        }
        if (result) {
            result = requestClipTimeWarp(clipId, speed / 100.0, pitchCompensate, changeDuration, undo, redo);
        }
        if (!result) {
            pCore->displayMessage(i18n("Change speed failed"), ErrorMessage);
            undo();
            TRACE_RES(false);
            return false;
        }
    } else {
        // If clip is not inserted on a track, we just change the producer
        result = m_allClips[clipId]->useTimewarpProducer(speed, pitchCompensate, changeDuration, undo, redo);
    }
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Change clip speed"));
    }
    TRACE_RES(result);
    return result;
}

const QString TimelineModel::getTrackTagById(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    bool isAudio = getTrackById_const(trackId)->isAudioTrack();
    int count = 1;
    int totalAudio = 2;
    auto it = m_allTracks.cbegin();
    bool found = false;
    while ((isAudio || !found) && it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack()) {
            totalAudio++;
            if (isAudio && !found) {
                count++;
            }
        } else if (!isAudio) {
            count++;
        }
        if ((*it)->getId() == trackId) {
            found = true;
        }
        it++;
    }
    return isAudio ? QStringLiteral("A%1").arg(totalAudio - count) : QStringLiteral("V%1").arg(count - 1);
}

void TimelineModel::updateProfile(Mlt::Profile *profile)
{
    m_profile = profile;
    m_tractor->set_profile(*m_profile);
    for (int i = 0; i < m_tractor->count(); i++) {
        std::shared_ptr<Mlt::Producer> tk(m_tractor->track(i));
        tk->set_profile(*m_profile);
        if (tk->type() == tractor_type) {
            Mlt::Tractor sub(*tk.get());
            for (int j = 0; j < sub.count(); j++) {
                std::shared_ptr<Mlt::Producer> subtk(sub.track(j));
                subtk->set_profile(*m_profile);
            }
        }
    }
    m_blackClip->set_profile(*m_profile);
    // Rebuild compositions since profile has changed
    buildTrackCompositing(true);
}

int TimelineModel::getBlankSizeNearClip(int clipId, bool after) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    int trackId = getClipTrackId(clipId);
    if (trackId != -1) {
        return getTrackById_const(trackId)->getBlankSizeNearClip(clipId, after);
    }
    return 0;
}

int TimelineModel::getPreviousTrackId(int trackId)
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    bool audioWanted = (*it)->isAudioTrack();
    while (it != m_allTracks.cbegin()) {
        --it;
        if ((*it)->isAudioTrack() == audioWanted) {
            return (*it)->getId();
        }
    }
    return trackId;
}

int TimelineModel::getNextTrackId(int trackId)
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    bool audioWanted = (*it)->isAudioTrack();
    while (it != m_allTracks.cend()) {
        ++it;
        if (it != m_allTracks.cend() && (*it)->isAudioTrack() == audioWanted) {
            break;
        }
    }
    return it == m_allTracks.cend() ? trackId : (*it)->getId();
}

bool TimelineModel::requestClearSelection(bool onDeletion)
{
    QWriteLocker locker(&m_lock);
    TRACE();
    if (m_currentSelection == -1) {
        TRACE_RES(true);
        return true;
    }
    if (isGroup(m_currentSelection)) {
        // Reset offset display on clips
        std::unordered_set<int> items = m_groups->getLeaves(m_currentSelection);
        for (auto &id : items) {
            if (isGroup(id)) {
                std::unordered_set<int> children = m_groups->getLeaves(id);
                items.insert(children.begin(), children.end());
            } else if (isClip(id)) {
                m_allClips[id]->clearOffset();
                m_allClips[id]->setGrab(false);
                m_allClips[id]->setSelected(false);
            } else if (isComposition(id)) {
                m_allCompositions[id]->setGrab(false);
                m_allCompositions[id]->setSelected(false);
            }
            if (m_groups->getType(m_currentSelection) == GroupType::Selection) {
                m_groups->destructGroupItem(m_currentSelection);
            }
        }
    } else {
        if (isClip(m_currentSelection)) {
            m_allClips[m_currentSelection]->setGrab(false);
            m_allClips[m_currentSelection]->setSelected(false);
        } else if (isComposition(m_currentSelection)) {
            m_allCompositions[m_currentSelection]->setGrab(false);
            m_allCompositions[m_currentSelection]->setSelected(false);
        }
        Q_ASSERT(onDeletion || isClip(m_currentSelection) || isComposition(m_currentSelection));
    }
    m_currentSelection = -1;
    emit selectionChanged();
    TRACE_RES(true);
    return true;
}
void TimelineModel::requestClearSelection(bool onDeletion, Fun &undo, Fun &redo)
{
    Fun operation = [this, onDeletion]() {
        requestClearSelection(onDeletion);
        return true;
    };
    Fun reverse = [this, clips = getCurrentSelection()]() { return requestSetSelection(clips); };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    }
}

std::unordered_set<int> TimelineModel::getCurrentSelection() const
{
    READ_LOCK();
    if (m_currentSelection == -1) {
        return {};
    }
    if (isGroup(m_currentSelection)) {
        return m_groups->getLeaves(m_currentSelection);
    } else {
        Q_ASSERT(isClip(m_currentSelection) || isComposition(m_currentSelection));
        return {m_currentSelection};
    }
}

void TimelineModel::requestAddToSelection(int itemId, bool clear)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, clear);
    if (clear) {
        requestClearSelection();
    }
    std::unordered_set<int> selection = getCurrentSelection();
    if (selection.count(itemId) == 0) {
        selection.insert(itemId);
        requestSetSelection(selection);
    }
}

void TimelineModel::requestRemoveFromSelection(int itemId)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId);
    std::unordered_set<int> all_items = {itemId};
    int parentGroup = m_groups->getDirectAncestor(itemId);
    if (parentGroup > -1 && m_groups->getType(parentGroup) != GroupType::Selection) {
        all_items = m_groups->getLeaves(parentGroup);
    }
    std::unordered_set<int> selection = getCurrentSelection();
    for (int current_itemId : all_items) {
        if (selection.count(current_itemId) > 0) {
            selection.erase(current_itemId);
        }
    }
    requestSetSelection(selection);
}

bool TimelineModel::requestSetSelection(const std::unordered_set<int> &ids)
{
    QWriteLocker locker(&m_lock);
    TRACE(ids);

    requestClearSelection();
    // if the items are in groups, we must retrieve their topmost containing groups
    std::unordered_set<int> roots;
    std::transform(ids.begin(), ids.end(), std::inserter(roots, roots.begin()), [&](int id) { return m_groups->getRootId(id); });

    bool result = true;
    if (roots.size() == 0) {
        m_currentSelection = -1;
    } else if (roots.size() == 1) {
        m_currentSelection = *(roots.begin());
        setSelected(m_currentSelection, true);
    } else {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        if (ids.size() == 2) {
            // Check if we selected 2 clips from the same master
            QList<int> pairIds;
            for (auto &id : roots) {
                if (isClip(id)) {
                    pairIds << id;
                }
            }
            if (pairIds.size() == 2 && getClipBinId(pairIds.at(0)) == getClipBinId(pairIds.at(1))) {
                // Check if they have same bin id
                ClipType::ProducerType type = m_allClips[pairIds.at(0)]->clipType();
                if (type == ClipType::AV || type == ClipType::Audio || type == ClipType::Video) {
                    // Both clips have same bin ID, display offset
                    int pos1 = getClipPosition(pairIds.at(0));
                    int pos2 = getClipPosition(pairIds.at(1));
                    if (pos2 > pos1) {
                        int offset = pos2 - getClipIn(pairIds.at(1)) - (pos1 - getClipIn(pairIds.at(0)));
                        if (offset != 0) {
                            m_allClips[pairIds.at(1)]->setOffset(offset);
                            m_allClips[pairIds.at(0)]->setOffset(-offset);
                        }
                    } else {
                        int offset = pos1 - getClipIn(pairIds.at(0)) - (pos2 - getClipIn(pairIds.at(1)));
                        if (offset != 0) {
                            m_allClips[pairIds.at(0)]->setOffset(offset);
                            m_allClips[pairIds.at(1)]->setOffset(-offset);
                        }
                    }
                }
            }
        }
        result = (m_currentSelection = m_groups->groupItems(ids, undo, redo, GroupType::Selection)) >= 0;
        Q_ASSERT(m_currentSelection >= 0);
    }
    emit selectionChanged();
    return result;
}

void TimelineModel::setSelected(int itemId, bool sel)
{
    if (isClip(itemId)) {
        m_allClips[itemId]->setSelected(sel);
    } else if (isComposition(itemId)) {
        m_allCompositions[itemId]->setSelected(sel);
    } else if (isGroup(itemId)) {
        auto leaves = m_groups->getLeaves(itemId);
        for (auto &id : leaves) {
            setSelected(id, true);
        }
    }
}

bool TimelineModel::requestSetSelection(const std::unordered_set<int> &ids, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Fun reverse = [this]() {
        requestClearSelection(false);
        return true;
    };
    Fun operation = [this, ids]() { return requestSetSelection(ids); };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

void TimelineModel::setTrackLockedState(int trackId, bool lock)
{
    QWriteLocker locker(&m_lock);
    TRACE(trackId, lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    Fun lock_lambda = [this, trackId]() {
        getTrackById(trackId)->lock();
        return true;
    };
    Fun unlock_lambda = [this, trackId]() {
        getTrackById(trackId)->unlock();
        return true;
    };
    if (lock) {
        if (lock_lambda()) {
            UPDATE_UNDO_REDO(lock_lambda, unlock_lambda, undo, redo);
            PUSH_UNDO(undo, redo, i18n("Lock track"));
        }
    } else {
        if (unlock_lambda()) {
            UPDATE_UNDO_REDO(unlock_lambda, lock_lambda, undo, redo);
            PUSH_UNDO(undo, redo, i18n("Unlock track"));
        }
    }
}

std::unordered_set<int> TimelineModel::getAllTracksIds() const
{
    READ_LOCK();
    std::unordered_set<int> result;
    std::transform(m_iteratorTable.begin(), m_iteratorTable.end(), std::inserter(result, result.begin()), [&](const auto &track) { return track.first; });
    return result;
}

void TimelineModel::switchComposition(int cid, const QString &compoId)
{
    Q_ASSERT(isComposition(cid));
    std::shared_ptr<CompositionModel> compo = m_allCompositions.at(cid);
    int currentPos = compo->getPosition();
    int duration = compo->getPlaytime();
    int currentTrack = compo->getCurrentTrackId();
    int a_track = compo->getATrack();
    int forcedTrack = compo->getForcedTrack();
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // Clear selection
    requestClearSelection(true);
    if (m_groups->isInGroup(cid)) {
        pCore->displayMessage(i18n("Cannot operate on grouped composition, please ungroup"), ErrorMessage);
        return;
    }

    bool res = requestCompositionDeletion(cid, undo, redo);
    int newId;
    res = res && requestCompositionInsertion(compoId, currentTrack, a_track, currentPos, duration, nullptr, newId, undo, redo);
    if (res) {
        if (forcedTrack > -1 && isComposition(newId)) {
            m_allCompositions[newId]->setForceTrack(true);
        }
        Fun local_redo = [newId, this]() {
            requestSetSelection({newId});
            return true;
        };
        Fun local_undo = [cid, this]() {
            requestSetSelection({cid});
            return true;
        };
        local_redo();
        PUSH_LAMBDA(local_redo, redo);
        PUSH_LAMBDA(local_undo, undo);
        PUSH_UNDO(undo, redo, i18n("Change composition"));
    } else {
        undo();
    }
}
