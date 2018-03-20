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
#include "groupsmodel.hpp"
#include "kdenlivesettings.h"
#include "snapmodel.hpp"
#include "timelinefunctions.hpp"
#include "trackmodel.hpp"

#include <QDebug>
#include <QModelIndex>
#include <klocalizedstring.h>
#include <mlt++/MltConsumer.h>
#include <mlt++/MltField.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>
#include <mlt++/MltTransition.h>
#include <queue>
#ifdef LOGGING
#include <sstream>
#include <utility>
#endif

#include "macros.hpp"

int TimelineModel::next_id = 0;

TimelineModel::TimelineModel(Mlt::Profile *profile, std::weak_ptr<DocUndoStack> undo_stack)
    : QAbstractItemModel_shared_from_this()
    , m_tractor(new Mlt::Tractor(*profile))
    , m_snaps(new SnapModel())
    , m_undoStack(undo_stack)
    , m_profile(profile)
    , m_blackClip(new Mlt::Producer(*profile, "color:black"))
    , m_lock(QReadWriteLock::Recursive)
    , m_timelineEffectsEnabled(true)
    , m_id(getNextId())
    , m_temporarySelectionGroup(-1)
    , m_overlayTrackCount(-1)
    , m_audioTarget(-1)
    , m_videoTarget(-1)
{
    // Create black background track
    m_blackClip->set("id", "black_track");
    m_blackClip->set("mlt_type", "producer");
    m_blackClip->set("aspect_ratio", 1);
    m_blackClip->set("length", INT_MAX);
    m_blackClip->set("set.test_audio", 0);
    m_blackClip->set("length", INT_MAX);
    m_blackClip->set_in_and_out(0, 10);
    m_tractor->insert_track(*m_blackClip, 0);

#ifdef LOGGING
    m_logFile = std::ofstream("log.txt");
    m_logFile << "TEST_CASE(\"Regression\") {" << std::endl;
    m_logFile << "Mlt::Profile profile;" << std::endl;
    m_logFile << "std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);" << std::endl;
    m_logFile << "std::shared_ptr<TimelineModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);" << std::endl;
    m_logFile << "TimelineModel::next_id = 0;" << std::endl;
    m_logFile << "int dummy_id;" << std::endl;
#endif
}

TimelineModel::~TimelineModel()
{
    std::vector<int> all_ids;
    for (auto tracks : m_iteratorTable) {
        all_ids.push_back(tracks.first);
    }
    for (auto tracks : all_ids) {
        deregisterTrack_lambda(tracks, false)();
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

int TimelineModel::getTrackIndexFromPosition(int pos) const
{
    Q_ASSERT(pos >= 0 && pos < (int)m_allTracks.size());
    READ_LOCK();
    auto it = m_allTracks.begin();
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
    Q_ASSERT(isClip(itemId) || isComposition(itemId));
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

int TimelineModel::getClipIn(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    int pos = clip->getIn();
    return pos;
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
    auto it = m_allTracks.begin();
    int pos = (int)std::distance(it, (decltype(it))m_iteratorTable.at(trackId));
    return pos;
}

int TimelineModel::getTrackMltIndex(int trackId) const
{
    READ_LOCK();
    // Because of the black track that we insert in first position, the mlt index is the position + 1
    return getTrackPosition(trackId) + 1;
}

QList<int> TimelineModel::getLowerTracksId(int trackId, TrackType type) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    QList<int> results;
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.begin()) {
        --it;
        if (type == TrackType::AnyTrack) {
            results << (*it)->getId();
            continue;
        }
        int audioTrack = (*it)->getProperty("kdenlive:audio_track").toInt();
        if (type == TrackType::AudioTrack && audioTrack == 1) {
            results << (*it)->getId();
        } else if (type == TrackType::VideoTrack && audioTrack == 0) {
            results << (*it)->getId();
        }
    }
    return results;
}

int TimelineModel::getPreviousVideoTrackPos(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.begin()) {
        --it;
        if (it != m_allTracks.begin() && (*it)->getProperty("kdenlive:audio_track").toInt() == 0) {
            break;
        }
    }
    return it == m_allTracks.begin() ? 0 : getTrackMltIndex((*it)->getId());
}

bool TimelineModel::requestClipMove(int clipId, int trackId, int position, bool updateView, bool invalidateTimeline, Fun &undo, Fun &redo)
{
    qDebug() << "// FINAL MOVE: " << invalidateTimeline << ", UPDATE VIEW: " << updateView;
    if (trackId == -1) {
        return false;
    }
    Q_ASSERT(isClip(clipId));
    if (getTrackById_const(trackId)->isLocked()) {
        return false;
    }
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    bool ok = true;
    int old_trackId = getClipTrackId(clipId);
    if (old_trackId != -1) {
        if (getTrackById_const(old_trackId)->isLocked()) {
            return false;
        }
        ok = getTrackById(old_trackId)->requestClipDeletion(clipId, updateView, invalidateTimeline, local_undo, local_redo);
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    ok = getTrackById(trackId)->requestClipInsertion(clipId, position, updateView, invalidateTimeline, local_undo, local_redo);
    if (!ok) {
        // qDebug()<<"-------------\n\nINSERTION FAILED, REVERTING\n\n-------------------";
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestClipMove(int clipId, int trackId, int position, bool updateView, bool logUndo, bool invalidateTimeline)
{
#ifdef LOGGING
    m_logFile << "timeline->requestClipMove(" << clipId << "," << trackId << " ," << position << ", " << (updateView ? "true" : "false") << ", "
              << (logUndo ? "true" : "false") << " ); " << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allClips.count(clipId) > 0);
    if (m_allClips[clipId]->getPosition() == position && getClipTrackId(clipId) == trackId) {
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
        return requestGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestClipMove(clipId, trackId, position, updateView, invalidateTimeline, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move clip"));
    }
    return res;
}

int TimelineModel::suggestClipMove(int clipId, int trackId, int position, int snapDistance)
{
#ifdef LOGGING
    m_logFile << "timeline->suggestClipMove(" << clipId << "," << trackId << " ," << position << "); " << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isClip(clipId));
    Q_ASSERT(isTrack(trackId));
    int currentPos = getClipPosition(clipId);
    if (currentPos == position) {
        return position;
    }
    bool after = position > currentPos;
    if (snapDistance > 0) {
        // For snapping, we must ignore all in/outs of the clips of the group being moved
        std::vector<int> ignored_pts;
        std::unordered_set<int> all_clips = {clipId};
        if (m_groups->isInGroup(clipId)) {
            int groupId = m_groups->getRootId(clipId);
            all_clips = m_groups->getLeaves(groupId);
        }
        for (int current_clipId : all_clips) {
            if (getItemTrackId(current_clipId) != -1) {
                int in = getItemPosition(current_clipId);
                int out = in + getItemPlaytime(current_clipId);
                ignored_pts.push_back(in);
                ignored_pts.push_back(out);
            }
        }

        int snapped = requestBestSnapPos(position, m_allClips[clipId]->getPlaytime(), ignored_pts, snapDistance);
        // qDebug() << "Starting suggestion " << clipId << position << currentPos << "snapped to " << snapped;
        if (snapped >= 0) {
            position = snapped;
        }
    }
    // we check if move is possible
    bool possible = requestClipMove(clipId, trackId, position, false, false, false);
    // bool possible = requestClipMove(clipId, trackId, position, false, false, undo, redo);
    if (possible) {
        return position;
    }
    // Find best possible move
    if (!m_groups->isInGroup(clipId)) {
        // Easy
        int blank_length = getTrackById(trackId)->getBlankSizeNearClip(clipId, after);
        qDebug() << "Found blank" << blank_length;
        if (blank_length < INT_MAX) {
            if (after) {
                position = currentPos + blank_length;
            } else {
                position = currentPos - blank_length;
            }
        } else {
            return false;
        }
        possible = requestClipMove(clipId, trackId, position, false, false, false);
        return possible ? position : currentPos;
    }
    // find best pos for groups
    int groupId = m_groups->getRootId(clipId);
    std::unordered_set<int> all_clips = m_groups->getLeaves(groupId);
    QMap<int, int> trackPosition;

    // First pass, sort clips by track and keep only the first / last depending on move direction
    for (int current_clipId : all_clips) {
        int clipTrack = getClipTrackId(current_clipId);
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
            trackPosition.insert(clipTrack, after ? in + getItemPlaytime(current_clipId) : in);
        }
    }
    // Now check space on each track
    QMapIterator<int, int> i(trackPosition);
    int blank_length = -1;
    while (i.hasNext()) {
        i.next();
        int track_space;
        if (!after) {
            // Check space before the position
            track_space = i.value() - getTrackById(i.key())->getBlankStart(i.value() - 1);
            if (blank_length == -1 || blank_length > track_space) {
                blank_length = track_space;
            }
        } else {
            // Check after before the position
            track_space = getTrackById(i.key())->getBlankEnd(i.value() + 1) - i.value();
            if (blank_length == -1 || blank_length > track_space) {
                blank_length = track_space;
            }
        }
    }
    if (blank_length != 0) {
        int updatedPos = currentPos + (after ? blank_length : -blank_length);
        possible = requestClipMove(clipId, trackId, updatedPos, false, false, false);
        if (possible) {
            return updatedPos;
        }
    }
    return currentPos;
}

int TimelineModel::suggestCompositionMove(int compoId, int trackId, int position, int snapDistance)
{
#ifdef LOGGING
    m_logFile << "timeline->suggestCompositionMove(" << compoId << "," << trackId << " ," << position << "); " << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isComposition(compoId));
    Q_ASSERT(isTrack(trackId));
    int currentPos = getCompositionPosition(compoId);
    int currentTrack = getCompositionTrackId(compoId);
    if (currentPos == position || currentTrack != trackId) {
        return position;
    }

    if (snapDistance > 0) {
        // For snapping, we must ignore all in/outs of the clips of the group being moved
        std::vector<int> ignored_pts;
        if (m_groups->isInGroup(compoId)) {
            int groupId = m_groups->getRootId(compoId);
            auto all_clips = m_groups->getLeaves(groupId);
            for (int current_compoId : all_clips) {
                // TODO: fix for composition
                int in = getItemPosition(current_compoId);
                int out = in + getItemPlaytime(current_compoId);
                ignored_pts.push_back(in);
                ignored_pts.push_back(out);
            }
        } else {
            int in = currentPos;
            int out = in + getCompositionPlaytime(compoId);
            qDebug() << " * ** IGNORING SNAP PTS: " << in << "-" << out;
            ignored_pts.push_back(in);
            ignored_pts.push_back(out);
        }

        int snapped = requestBestSnapPos(position, m_allCompositions[compoId]->getPlaytime(), ignored_pts, snapDistance);
        qDebug() << "Starting suggestion " << compoId << position << currentPos << "snapped to " << snapped;
        if (snapped >= 0) {
            position = snapped;
        }
    }
    // we check if move is possible
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool possible = requestCompositionMove(compoId, trackId, m_allCompositions[compoId]->getForcedTrack(), position, false, undo, redo);
    qDebug() << "Original move success" << possible;
    if (possible) {
        bool undone = undo();
        Q_ASSERT(undone);
        return position;
    }
    bool after = position > currentPos;
    int blank_length = getTrackById(trackId)->getBlankSizeNearComposition(compoId, after);
    qDebug() << "Found blank" << blank_length;
    if (blank_length < INT_MAX) {
        if (after) {
            return currentPos + blank_length;
        }
        return currentPos - blank_length;
    }
    return position;
}

bool TimelineModel::requestClipInsertion(const QString &binClipId, int trackId, int position, int &id, bool logUndo, bool refreshView)
{
#ifdef LOGGING
    m_logFile << "timeline->requestClipInsertion(" << binClipId.toStdString() << "," << trackId << " ," << position << ", dummy_id );" << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestClipInsertion(binClipId, trackId, position, id, logUndo, refreshView, undo, redo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Insert Clip"));
    }
    return result;
}

bool TimelineModel::requestClipCreation(const QString &binClipId, int &id, PlaylistState::ClipState state, Fun &undo, Fun &redo)
{
    int clipId = TimelineModel::getNextId();
    id = clipId;
    Fun local_undo = deregisterClip_lambda(clipId);
    QString bid = binClipId;
    if (binClipId.contains(QLatin1Char('/'))) {
        bid = binClipId.section(QLatin1Char('/'), 0, 0);
    }
    if (!pCore->projectItemModel()->hasClip(bid)) {
        return false;
    }
    ClipModel::construct(shared_from_this(), bid, clipId, state);
    auto clip = m_allClips[clipId];
    Fun local_redo = [clip, this, state]() {
        // We capture a shared_ptr to the clip, which means that as long as this undo object lives, the clip object is not deleted. To insert it back it is
        // sufficient to register it.
        registerClip(clip);
        clip->refreshProducerFromBin(state);
        return true;
    };

    if (binClipId.contains(QLatin1Char('/'))) {
        int in = binClipId.section(QLatin1Char('/'), 1, 1).toInt();
        int out = binClipId.section(QLatin1Char('/'), 2, 2).toInt();
        int initLength = m_allClips[clipId]->getPlaytime();
        bool res = requestItemResize(clipId, initLength - in, false, true, local_undo, local_redo);
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

bool TimelineModel::requestClipInsertion(const QString &binClipId, int trackId, int position, int &id, bool logUndo, bool refreshView, Fun &undo, Fun &redo)
{
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    bool res = false;
    if (getTrackById_const(trackId)->isLocked()) {
        return false;
    }
    ClipType type = ClipType::Unknown;
    if (KdenliveSettings::splitaudio()) {
        QString bid = binClipId.section(QLatin1Char('/'), 0, 0);
        if (!pCore->projectItemModel()->hasClip(bid)) {
            return false;
        }
        std::shared_ptr<ProjectClip> master = pCore->projectItemModel()->getClipByBinID(bid);
        type = master->clipType();
    }
    if (type == ClipType::AV) {
        bool audioDrop = getTrackById_const(trackId)->isAudioTrack();
        res = requestClipCreation(binClipId, id, audioDrop ? PlaylistState::AudioOnly : PlaylistState::VideoOnly, local_undo, local_redo);
        res = res && requestClipMove(id, trackId, position, refreshView, logUndo, local_undo, local_redo);
        if (res && logUndo && !audioDrop) {
            QList<int> possibleTracks = m_audioTarget >= 0 ? QList<int>() << m_audioTarget : getLowerTracksId(trackId, TrackType::AudioTrack);
            if (possibleTracks.isEmpty()) {
                // No available audio track for splitting, abort
                pCore->displayMessage(i18n("No available audio track for split operation"), ErrorMessage);
                res = false;
            } else {
                std::function<bool(void)> audio_undo = []() { return true; };
                std::function<bool(void)> audio_redo = []() { return true; };
                int newId;
                res = requestClipCreation(binClipId, newId, PlaylistState::AudioOnly, audio_undo, audio_redo);
                if (res) {
                    bool move = false;
                    while (!move && !possibleTracks.isEmpty()) {
                        int newTrack = possibleTracks.takeFirst();
                        move = requestClipMove(newId, newTrack, position, true, false, audio_undo, audio_redo);
                    }
                    // use lazy evaluation to group only if move was successful
                    res = res && move && requestClipsGroup({id, newId}, audio_undo, audio_redo, GroupType::AVSplit);
                    if (!res || !move) {
                        pCore->displayMessage(i18n("Audio split failed: no viable track"), ErrorMessage);
                        bool undone = audio_undo();
                        Q_ASSERT(undone);
                    } else {
                        UPDATE_UNDO_REDO(audio_redo, audio_undo, local_undo, local_redo);
                    }
                } else {
                    pCore->displayMessage(i18n("Audio split failed: impossible to create audio clip"), ErrorMessage);
                    bool undone = audio_undo();
                    Q_ASSERT(undone);
                }
            }
        }
    } else {
        res = requestClipCreation(binClipId, id, PlaylistState::Original, local_undo, local_redo);
        res = res && requestClipMove(id, trackId, position, refreshView, logUndo, local_undo, local_redo);
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

bool TimelineModel::requestItemDeletion(int clipId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (m_groups->isInGroup(clipId)) {
        return requestGroupDeletion(clipId, undo, redo);
    }
    return requestClipDeletion(clipId, undo, redo);

}

bool TimelineModel::requestItemDeletion(int itemId, bool logUndo)
{
#ifdef LOGGING
    m_logFile << "timeline->requestItemDeletion(" << itemId << "); " << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isClip(itemId) || isComposition(itemId));
    if (m_groups->isInGroup(itemId)) {
        return requestGroupDeletion(itemId, logUndo);
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = false;
    QString actionLabel;
    if (isClip(itemId)) {
        actionLabel = i18n("Delete Clip");
        res = requestClipDeletion(itemId, undo, redo);
    } else {
        actionLabel = i18n("Delete Composition");
        res = requestCompositionDeletion(itemId, undo, redo);
    }
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, actionLabel);
    }
    return res;
}

bool TimelineModel::requestClipDeletion(int clipId, Fun &undo, Fun &redo)
{
    int trackId = getClipTrackId(clipId);
    if (trackId != -1) {
        bool res = getTrackById(trackId)->requestClipDeletion(clipId, true, true, undo, redo);
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
        registerClip(clip);
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
        bool res = getTrackById(trackId)->requestCompositionDeletion(compositionId, true, undo, redo);
        if (!res) {
            undo();
            return false;
        } else {
            unplantComposition(compositionId);
        }
    }
    Fun operation = deregisterComposition_lambda(compositionId);
    auto composition = m_allCompositions[compositionId];
    Fun reverse = [this, composition]() {
        // We capture a shared_ptr to the composition, which means that as long as this undo object lives, the composition object is not deleted. To insert it
        // back it is sufficient to register it.
        registerComposition(composition);
        return true;
    };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    undo();
    return false;
}

std::unordered_set<int> TimelineModel::getItemsAfterPosition(int trackId, int position, int end, bool listCompositions)
{
    Q_UNUSED(listCompositions)

    std::unordered_set<int> allClips;
    auto it = m_allTracks.cbegin();
    if (trackId == -1) {
        while (it != m_allTracks.cend()) {
            std::unordered_set<int> clipTracks = (*it)->getClipsAfterPosition(position, end);
            allClips.insert(clipTracks.begin(), clipTracks.end());
            ++it;
        }
    } else {
        int target_track_position = getTrackPosition(trackId);
        std::advance(it, target_track_position);
        std::unordered_set<int> clipTracks = (*it)->getClipsAfterPosition(position, end);
        allClips.insert(clipTracks.begin(), clipTracks.end());
    }
    return allClips;
}

bool TimelineModel::requestGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool logUndo)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move group"));
    }
    return res;
}

bool TimelineModel::requestGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool finalMove, Fun &undo, Fun &redo)
{
    Q_UNUSED(clipId)
#ifdef LOGGING
    m_logFile << "timeline->requestGroupMove(" << clipId << "," << groupId << " ," << delta_track << ", " << delta_pos << ", "
              << (updateView ? "true" : "false") << " ); " << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allGroups.count(groupId) > 0);
    bool ok = true;
    auto all_clips = m_groups->getLeaves(groupId);
    std::vector<int> sorted_clips(all_clips.begin(), all_clips.end());
    // we have to sort clip in an order that allows to do the move without self conflicts
    // If we move up, we move first the clips on the upper tracks (and conversely).
    // If we move left, we move first the leftmost clips (and conversely).
    std::sort(sorted_clips.begin(), sorted_clips.end(), [delta_track, delta_pos, this](int clipId1, int clipId2) {
        int trackId1 = getItemTrackId(clipId1);
        int trackId2 = getItemTrackId(clipId2);
        int track_pos1 = getTrackPosition(trackId1);
        int track_pos2 = getTrackPosition(trackId2);
        if (trackId1 == trackId2) {
            int p1 = isClip(clipId1) ? m_allClips[clipId1]->getPosition() : m_allCompositions[clipId1]->getPosition();
            int p2 = isClip(clipId2) ? m_allClips[clipId2]->getPosition() : m_allCompositions[clipId2]->getPosition();
            return !(p1 <= p2) == !(delta_pos <= 0);
        }
        return !(track_pos1 <= track_pos2) == !(delta_track <= 0);
    });
    for (int clip : sorted_clips) {
        int current_track_id = getItemTrackId(clip);
        int current_track_position = getTrackPosition(current_track_id);
        int target_track_position = current_track_position + delta_track;
        if (target_track_position >= 0 && target_track_position < getTracksCount()) {
            auto it = m_allTracks.cbegin();
            std::advance(it, target_track_position);
            int target_track = (*it)->getId();
            if (isClip(clip)) {
                int target_position = m_allClips[clip]->getPosition() + delta_pos;
                ok = requestClipMove(clip, target_track, target_position, updateView, finalMove, undo, redo);
            } else {
                int target_position = m_allCompositions[clip]->getPosition() + delta_pos;
                ok = requestCompositionMove(clip, target_track, m_allCompositions[clip]->getForcedTrack(), target_position, updateView, undo, redo);
            }
        } else {
            qDebug() << "// ABORTING; MOVE TRIED ON TRACK: " << target_track_position << "..\n..\n..";
            ok = false;
        }
        if (!ok) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    return true;
}

bool TimelineModel::requestGroupDeletion(int clipId, bool logUndo)
{
#ifdef LOGGING
    m_logFile << "timeline->requestGroupDeletion(" << clipId << " ); " << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestGroupDeletion(clipId, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Remove group"));
    }
    return res;
}

bool TimelineModel::requestGroupDeletion(int clipId, Fun &undo, Fun &redo)
{
    // we do a breadth first exploration of the group tree, ungroup (delete) every inner node, and then delete all the leaves.
    std::queue<int> group_queue;
    group_queue.push(m_groups->getRootId(clipId));
    std::unordered_set<int> all_clips;
    std::unordered_set<int> all_compositions;
    while (!group_queue.empty()) {
        int current_group = group_queue.front();
        if (m_temporarySelectionGroup == current_group) {
            m_temporarySelectionGroup = -1;
        }
        group_queue.pop();
        Q_ASSERT(isGroup(current_group));
        auto children = m_groups->getDirectChildren(current_group);
        int one_child = -1; // we need the id on any of the indices of the elements of the group
        for (int c : children) {
            if (isClip(c)) {
                all_clips.insert(c);
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
            bool res = m_groups->ungroupItem(one_child, undo, redo);
            if (!res) {
                undo();
                return false;
            }
        }
    }
    for (int clip : all_clips) {
        bool res = requestClipDeletion(clip, undo, redo);
        if (!res) {
            undo();
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

int TimelineModel::requestItemResize(int itemId, int size, bool right, bool logUndo, int snapDistance, bool allowSingleResize)
{
#ifdef LOGGING
    m_logFile << "timeline->requestItemResize(" << itemId << "," << size << " ," << (right ? "true" : "false") << ", " << (logUndo ? "true" : "false") << ", "
              << (snapDistance > 0 ? "true" : "false") << " ); " << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isClip(itemId) || isComposition(itemId));
    if (size <= 0) return -1;
    int in = getItemPosition(itemId);
    int out = in + getItemPlaytime(itemId);
    if (snapDistance > 0) {
        Fun temp_undo = []() { return true; };
        Fun temp_redo = []() { return true; };
        int proposed_size = m_snaps->proposeSize(in, out, size, right, snapDistance);
        if (proposed_size < 0) {
            proposed_size = size;
        }
        bool success = false;
        if (isClip(itemId)) {
            success = m_allClips[itemId]->requestResize(proposed_size, right, temp_undo, temp_redo);
        } else {
            success = m_allCompositions[itemId]->requestResize(proposed_size, right, temp_undo, temp_redo);
        }
        if (success) {
            temp_undo(); // undo temp move
            size = proposed_size;
        }
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::unordered_set<int> all_items;
    if (!allowSingleResize && m_groups->isInGroup(itemId)) {
        int groupId = m_groups->getRootId(itemId);
        auto items = m_groups->getLeaves(groupId);
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
        result = result && requestItemResize(id, size, right, logUndo, undo, redo);
    }
    if (!result) {
        bool undone = undo();
        Q_ASSERT(undone);
        return -1;
    }
    if (result && logUndo) {
        if (isClip(itemId)) {
            PUSH_UNDO(undo, redo, i18n("Resize clip"));
        } else {
            PUSH_UNDO(undo, redo, i18n("Resize composition"));
        }
    }
    return result ? size : -1;
}

bool TimelineModel::requestItemResize(int itemId, int size, bool right, bool logUndo, Fun &undo, Fun &redo, bool blockUndo)
{
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    Fun update_model = [itemId, right, logUndo, this]() {
        Q_ASSERT(isClip(itemId) || isComposition(itemId));
        if (getItemTrackId(itemId) != -1) {
            QModelIndex modelIndex = isClip(itemId) ? makeClipIndexFromID(itemId) : makeCompositionIndexFromID(itemId);
            notifyChange(modelIndex, modelIndex, !right, true, logUndo);
        }
        return true;
    };
    bool result = false;
    if (isClip(itemId)) {
        result = m_allClips[itemId]->requestResize(size, right, local_undo, local_redo, logUndo);
    } else {
        Q_ASSERT(isComposition(itemId));
        result = m_allCompositions[itemId]->requestResize(size, right, local_undo, local_redo);
    }
    if (result) {
        if (!blockUndo) {
            PUSH_LAMBDA(update_model, local_undo);
        }
        PUSH_LAMBDA(update_model, local_redo);
        update_model();
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    return result;
}

int TimelineModel::requestClipsGroup(const std::unordered_set<int> &ids, bool logUndo, GroupType type)
{
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (m_temporarySelectionGroup > -1) {
        int firstChild = *m_groups->getDirectChildren(m_temporarySelectionGroup).begin();
        requestClipUngroup(firstChild, undo, redo);
        m_temporarySelectionGroup = -1;
    }
    int result = requestClipsGroup(ids, undo, redo, type);
    if (type == GroupType::Selection) {
        m_temporarySelectionGroup = result;
    }
    if (result > -1 && logUndo && type != GroupType::Selection) {
        PUSH_UNDO(undo, redo, i18n("Group clips"));
    }
    return result;
}

int TimelineModel::requestClipsGroup(const std::unordered_set<int> &ids, Fun &undo, Fun &redo, GroupType type)
{
#ifdef LOGGING
    std::stringstream group;
    m_logFile << "{" << std::endl;
    m_logFile << "auto group = {";
    bool deb = true;
    for (int clipId : ids) {
        if (deb)
            deb = false;
        else
            group << ", ";
        group << clipId;
    }
    m_logFile << group.str() << "};" << std::endl;
    m_logFile << "timeline->requestClipsGroup(group);" << std::endl;
    m_logFile << std::endl << "}" << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    for (int id : ids) {
        if (isClip(id)) {
            if (getClipTrackId(id) == -1) {
                return -1;
            }
        } else if (isComposition(id)) {
            if (getCompositionTrackId(id) == -1) {
                return -1;
            }
        } else if (!isGroup(id)) {
            return -1;
        }
    }
    int groupId = m_groups->groupItems(ids, undo, redo, type);
    if (type == GroupType::Selection && *(ids.begin()) == groupId) {
        // only one element selected, no group created
        return -1;
    }
    return groupId;
}

bool TimelineModel::requestClipUngroup(int id, bool logUndo)
{
#ifdef LOGGING
    m_logFile << "timeline->requestClipUngroup(" << id << " ); " << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestClipUngroup(id, undo, redo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Ungroup clips"));
    }
    if (id == m_temporarySelectionGroup) {
        m_temporarySelectionGroup = -1;
    }
    return result;
}

bool TimelineModel::requestClipUngroup(int id, Fun &undo, Fun &redo)
{
    return m_groups->ungroupItem(id, undo, redo);
}

bool TimelineModel::requestTrackInsertion(int position, int &id, const QString &trackName, bool audioTrack)
{
#ifdef LOGGING
    m_logFile << "timeline->requestTrackInsertion(" << position << ", dummy_id ); " << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestTrackInsertion(position, id, trackName, audioTrack, undo, redo);
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Insert Track"));
    }
    return result;
}

bool TimelineModel::requestTrackInsertion(int position, int &id, const QString &trackName, bool audioTrack, Fun &undo, Fun &redo, bool updateView)
{
    // TODO: make sure we disable overlayTrack before inserting a track
    if (position == -1) {
        position = (int)(m_allTracks.size());
    }
    if (position < 0 || position > (int)m_allTracks.size()) {
        return false;
    }
    int trackId = TimelineModel::getNextId();
    id = trackId;
    Fun local_undo = deregisterTrack_lambda(trackId, true);
    TrackModel::construct(shared_from_this(), trackId, position, trackName, audioTrack);
    auto track = getTrackById(trackId);
    Fun local_redo = [track, position, updateView, this]() {
        // We capture a shared_ptr to the track, which means that as long as this undo object lives, the track object is not deleted. To insert it back it is
        // sufficient to register it.
        registerTrack(track, position, updateView);
        return true;
    };
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestTrackDeletion(int trackId)
{
// TODO: make sure we disable overlayTrack before deleting a track
#ifdef LOGGING
    m_logFile << "timeline->requestTrackDeletion(" << trackId << "); " << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestTrackDeletion(trackId, undo, redo);
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Delete Track"));
    }
    return result;
}

bool TimelineModel::requestTrackDeletion(int trackId, Fun &undo, Fun &redo)
{
    Q_ASSERT(isTrack(trackId));
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
    int old_position = getTrackPosition(trackId);
    auto operation = deregisterTrack_lambda(trackId, true);
    std::shared_ptr<TrackModel> track = getTrackById(trackId);
    Fun reverse = [this, track, old_position]() {
        // We capture a shared_ptr to the track, which means that as long as this undo object lives, the track object is not deleted. To insert it back it is
        // sufficient to register it.
        registerTrack(track, old_position);
        return true;
    };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, local_undo, local_redo);
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    local_undo();
    return false;
}

void TimelineModel::registerTrack(std::shared_ptr<TrackModel> track, int pos, bool doInsert, bool reloadView)
{
    qDebug() << "REGISTER TRACK" << track->getId() << pos;
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
    auto it = m_allTracks.insert(posIt, std::move(track));
    // it now contains the iterator to the inserted element, we store it
    Q_ASSERT(m_iteratorTable.count(id) == 0); // check that id is not used (shouldn't happen)
    m_iteratorTable[id] = it;
    if (reloadView) {
        // don't reload view on each track load on project opening
        _resetView();
    }
}

void TimelineModel::registerClip(const std::shared_ptr<ClipModel> &clip)
{
    int id = clip->getId();
    qDebug() << " // /REQUEST TL CLP REGSTR: " << id << "\n--------\nCLIPS COUNT: " << m_allClips.size();
    Q_ASSERT(m_allClips.count(id) == 0);
    m_allClips[id] = clip;
    clip->registerClipToBin();
    m_groups->createGroupItem(id);
    clip->setTimelineEffectsEnabled(m_timelineEffectsEnabled);
}

void TimelineModel::registerGroup(int groupId)
{
    Q_ASSERT(m_allGroups.count(groupId) == 0);
    m_allGroups.insert(groupId);
}

Fun TimelineModel::deregisterTrack_lambda(int id, bool updateView)
{
    return [this, id, updateView]() {
        qDebug() << "DEREGISTER TRACK" << id;
        auto it = m_iteratorTable[id];                        // iterator to the element
        int index = getTrackPosition(id);                     // compute index in list
        m_tractor->remove_track(static_cast<int>(index + 1)); // melt operation, add 1 to account for black background track
        // send update to the model
        m_allTracks.erase(it);     // actual deletion of object
        m_iteratorTable.erase(id); // clean table
        if (updateView) {
            QModelIndex root;
            _resetView();
        }
        return true;
    };
}

Fun TimelineModel::deregisterClip_lambda(int clipId)
{
    return [this, clipId]() {
        // qDebug() << " // /REQUEST TL CLP DELETION: " << clipId << "\n--------\nCLIPS COUNT: " << m_allClips.size();
        clearAssetView(clipId);
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
    return (*m_iteratorTable.at(trackId))->addEffect(effectId);
}

std::shared_ptr<ClipModel> TimelineModel::getClipPtr(int clipId) const
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId);
}

bool TimelineModel::addClipEffect(int clipId, const QString &effectId)
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId)->addEffect(effectId);
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
    int current = m_blackClip->get_playtime();
    int duration = 10;
    for (const auto &tck : m_iteratorTable) {
        auto track = (*tck.second);
        duration = qMax(duration, track->trackDuration());
    }
    if (duration != current) {
        // update black track length
        m_blackClip->set_in_and_out(0, duration);
        emit durationUpdated();
    }
}

int TimelineModel::duration() const
{
    return m_tractor->get_playtime();
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

int TimelineModel::requestBestSnapPos(int pos, int length, const std::vector<int> &pts, int snapDistance)
{
    if (!pts.empty()) {
        m_snaps->ignore(pts);
    }
    int snapped_start = m_snaps->getClosestPoint(pos);
    int snapped_end = m_snaps->getClosestPoint(pos + length);
    m_snaps->unIgnore();

    int startDiff = qAbs(pos - snapped_start);
    int endDiff = qAbs(pos + length - snapped_end);
    if (startDiff < endDiff && startDiff <= snapDistance) {
        // snap to start
        return snapped_start;
    }
    if (endDiff <= snapDistance) {
        // snap to end
        return snapped_end - length;
    }
    return -1;
}

int TimelineModel::requestNextSnapPos(int pos)
{
    return m_snaps->getNextPoint(pos);
}

int TimelineModel::requestPreviousSnapPos(int pos)
{
    return m_snaps->getPreviousPoint(pos);
}

void TimelineModel::addSnap(int pos)
{
    return m_snaps->addPoint(pos);
}

void TimelineModel::removeSnap(int pos)
{
    return m_snaps->removePoint(pos);
}

void TimelineModel::registerComposition(const std::shared_ptr<CompositionModel> &composition)
{
    int id = composition->getId();
    Q_ASSERT(m_allCompositions.count(id) == 0);
    m_allCompositions[id] = composition;
    m_groups->createGroupItem(id);
}

bool TimelineModel::requestCompositionInsertion(const QString &transitionId, int trackId, int position, int length, Mlt::Properties *transProps, int &id,
                                                bool logUndo)
{
#ifdef LOGGING
    m_logFile << "timeline->requestCompositionInsertion(\"composite\"," << trackId << " ," << position << "," << length << ", dummy_id );" << std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestCompositionInsertion(transitionId, trackId, -1, position, length, transProps, id, undo, redo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Insert Composition"));
    }
    return result;
}

bool TimelineModel::requestCompositionInsertion(const QString &transitionId, int trackId, int compositionTrack, int position, int length,
                                                Mlt::Properties *transProps, int &id, Fun &undo, Fun &redo)
{
    qDebug() << "Inserting compo track" << trackId << "pos" << position << "length" << length;
    int compositionId = TimelineModel::getNextId();
    id = compositionId;
    Fun local_undo = deregisterComposition_lambda(compositionId);
    CompositionModel::construct(shared_from_this(), transitionId, compositionId, transProps);
    auto composition = m_allCompositions[compositionId];
    Fun local_redo = [composition, this]() {
        // We capture a shared_ptr to the composition, which means that as long as this undo object lives, the composition object is not deleted. To insert it
        // back it is sufficient to register it.
        registerComposition(composition);
        return true;
    };
    bool res = requestCompositionMove(compositionId, trackId, compositionTrack, position, true, local_undo, local_redo);
    qDebug() << "trying to move" << trackId << "pos" << position << "succes " << res;
    if (res) {
        res = requestItemResize(compositionId, length, true, true, local_undo, local_redo, true);
        qDebug() << "trying to resize" << compositionId << "length" << length << "succes " << res;
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

int TimelineModel::getTrackCompositionsCount(int compoId) const
{
    return getTrackById_const(compoId)->getCompositionsCount();
}

bool TimelineModel::requestCompositionMove(int compoId, int trackId, int position, bool updateView, bool logUndo)
{
#ifdef LOGGING
    m_logFile << "timeline->requestCompositionMove(" << compoId << "," << trackId << " ," << position << ", " << (updateView ? "true" : "false") << ", "
              << (logUndo ? "true" : "false") << " ); " << std::endl;
#endif
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
        return requestGroupMove(compoId, groupId, delta_track, delta_pos, updateView, logUndo);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int min = getCompositionPosition(compoId);
    int max = min + getCompositionPlaytime(compoId);
    int tk = getCompositionTrackId(compoId);
    bool res = requestCompositionMove(compoId, trackId, m_allCompositions[compoId]->getForcedTrack(), position, updateView, undo, redo);
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

bool TimelineModel::requestCompositionMove(int compoId, int trackId, int compositionTrack, int position, bool updateView, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isComposition(compoId));
    Q_ASSERT(isTrack(trackId));
    if (compositionTrack == -1 || (compositionTrack > 0 && trackId == getTrackIndexFromPosition(compositionTrack - 1))) {
        qDebug() << "// compo track: " << trackId << ", PREVIOUS TK: " << getPreviousVideoTrackPos(trackId);
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
            UPDATE_UNDO_REDO(delete_operation, delete_reverse, local_undo, local_redo);
            ok = getTrackById(old_trackId)->requestCompositionDeletion(compoId, updateView, local_undo, local_redo);
        }
        if (!ok) {
            qDebug() << "Move failed because of first deletion request";
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    ok = getTrackById(trackId)->requestCompositionInsertion(compoId, position, updateView, local_undo, local_redo);
    if (!ok) qDebug() << "Move failed because of second insertion request";
    if (ok) {
        Fun insert_operation = []() { return true; };
        Fun insert_reverse = []() { return true; };
        if (old_trackId != trackId) {
            insert_operation = [this, compoId, trackId, compositionTrack, updateView]() {
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
            UPDATE_UNDO_REDO(insert_operation, insert_reverse, local_undo, local_redo);
        }
    }
    if (!ok) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::replantCompositions(int currentCompo, bool updateView)
{
    // We ensure that the compositions are planted in a decreasing order of b_track.
    // For that, there is no better option than to disconnect every composition and then reinsert everything in the correct order.
    std::vector<std::pair<int, int>> compos;
    for (const auto &compo : m_allCompositions) {
        int trackId = compo.second->getCurrentTrackId();
        if (trackId == -1 || compo.second->getATrack() == -1) {
            continue;
        }
        // Note: we need to retrieve the position of the track, that is its melt index.
        int trackPos = getTrackMltIndex(trackId);
        compos.push_back({trackPos, compo.first});
        if (compo.first != currentCompo) {
            unplantComposition(compo.first);
        }
    }
    // sort by decreasing b_track
    std::sort(compos.begin(), compos.end(), [](const std::pair<int, int> &a, const std::pair<int, int> &b) { return a.first > b.first; });
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
        Q_ASSERT(aTrack != -1);

        int ret = field->plant_transition(*m_allCompositions[compo.second].get(), aTrack, compo.first);
        qDebug() << "Planting composition " << compo.second << "in " << aTrack << "/" << compo.first << "IN = " << m_allCompositions[compo.second]->getIn()
                 << "OUT = " << m_allCompositions[compo.second]->getOut() << "ret=" << ret;

        Mlt::Transition &transition = *m_allCompositions[compo.second].get();
        transition.set_tracks(aTrack, compo.first);
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
        QVector<int> roles;
        roles.push_back(ItemATrack);
        notifyChange(modelIndex, modelIndex, roles);
    }
    return true;
}

bool TimelineModel::unplantComposition(int compoId)
{
    qDebug() << "Unplanting" << compoId;
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
            qDebug() << "Constistency check failed for track" << tck.first;
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
    }
    // Check snaps
    auto stored_snaps = m_snaps->_snaps();
    if (snaps.size() != stored_snaps.size()) {
        qDebug() << "Wrong number of snaps";
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
            mlt_transition tr = (mlt_transition)nextservice;
            int currentTrack = mlt_transition_get_b_track(tr);
            int currentATrack = mlt_transition_get_a_track(tr);

            int currentIn = (int)mlt_transition_get_in(tr);
            int currentOut = (int)mlt_transition_get_out(tr);

            qDebug() << "looking composition IN: " << currentIn << ", OUT: " << currentOut << ", TRACK: " << currentTrack << " / " << currentATrack;
            int foundId = -1;
            // we iterate to try to find a matching compo
            for (int compoId : remaining_compo) {
                if (getTrackMltIndex(getCompositionTrackId(compoId)) == currentTrack &&
                    getTrackMltIndex(m_allCompositions[compoId]->getATrack()) == currentATrack && m_allCompositions[compoId]->getIn() == currentIn &&
                    m_allCompositions[compoId]->getOut() == currentOut) {
                    foundId = compoId;
                    break;
                }
            }
            if (foundId == -1) {
                qDebug() << "Error, we didn't find matching composition IN: " << currentIn << ", OUT: " << currentOut << ", TRACK: " << currentTrack << " / "
                         << currentATrack;
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
    return true;
}

bool TimelineModel::requestItemResizeToPos(int itemId, int position, bool right)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isClip(itemId) || isComposition(itemId));
    int in, out;
    if (isClip(itemId)) {
        in = getClipPosition(itemId);
        out = in + getClipPlaytime(itemId) - 1;
    } else {
        in = getCompositionPosition(itemId);
        out = in + getCompositionPlaytime(itemId) - 1;
    }
    int size = 0;
    if (!right) {
        if (position < out) {
            size = out - position;
        }
    } else {
        if (position > in) {
            size = position - in;
        }
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestItemResize(itemId, size, right, true, undo, redo);
    if (result) {
        if (isClip(itemId)) {
            PUSH_UNDO(undo, redo, i18n("Resize clip"));
        } else {
            PUSH_UNDO(undo, redo, i18n("Resize composition"));
        }
    }
    return result;
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

Mlt::Producer *TimelineModel::producer()
{
    auto *prod = new Mlt::Producer(tractor());
    return prod;
}

void TimelineModel::checkRefresh(int start, int end)
{
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
    pCore->adjustAssetRange(clipId, in, out);
}

void TimelineModel::requestClipReload(int clipId)
{
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    m_allClips[clipId]->refreshProducerFromBin();

    // in order to make the producer change effective, we need to unplant / replant the clip in int track
    int old_trackId = getClipTrackId(clipId);
    int oldPos = getClipPosition(clipId);
    if (old_trackId != -1) {
        getTrackById(old_trackId)->requestClipDeletion(clipId, false, true, local_undo, local_redo);
        getTrackById(old_trackId)->requestClipInsertion(clipId, oldPos, true, true, local_undo, local_redo);
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

bool TimelineModel::requestClipTimeWarp(int clipId, int trackId, int blankSpace, double speed, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    int oldPos = getClipPosition(clipId);
    // in order to make the producer change effective, we need to unplant / replant the clip in int track
    bool success = true;
    success = getTrackById(trackId)->requestClipDeletion(clipId, true, true, local_undo, local_redo);
    if (success) {
        success = m_allClips[clipId]->useTimewarpProducer(speed, blankSpace, local_undo, local_redo);
    }
    if (success) {
        success = getTrackById(trackId)->requestClipInsertion(clipId, oldPos, true, true, local_undo, local_redo);
    }
    PUSH_LAMBDA(local_undo, undo);
    PUSH_LAMBDA(local_redo, redo);
    return success;
}

bool TimelineModel::changeItemSpeed(int clipId, int speed)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // Get main clip info
    int trackId = getClipTrackId(clipId);
    int splitId = -1;
    // Check if clip has a split partner
    bool result = true;
    if (trackId != -1) {
        int blankSpace = getTrackById(trackId)->getBlankSizeNearClip(clipId, true);
        splitId = m_groups->getSplitPartner(clipId);
        bool success = true;
        if (splitId > -1) {
            int split_trackId = getClipTrackId(splitId);
            blankSpace = qMin(blankSpace, getTrackById(split_trackId)->getBlankSizeNearClip(splitId, true));
            result = requestClipTimeWarp(splitId, split_trackId, blankSpace, speed / 100.0, undo, redo);
        }
        if (result) {
            result = requestClipTimeWarp(clipId, trackId, blankSpace, speed / 100.0, undo, redo);
        }
    } else {
        m_allClips[clipId]->useTimewarpProducer(speed, -1, undo, redo);
    }
    if (result) {
        QVector<int> roles;
        roles.push_back(SpeedRole);
        Fun update_model = [clipId, roles, this]() {
            QModelIndex modelIndex = makeClipIndexFromID(clipId);
            notifyChange(modelIndex, modelIndex, roles);
            return true;
        };
        PUSH_LAMBDA(update_model, undo);
        PUSH_LAMBDA(update_model, redo);
        if (splitId > -1) {
            Fun update_model2 = [splitId, roles, this]() {
                QModelIndex modelIndex = makeClipIndexFromID(splitId);
                notifyChange(modelIndex, modelIndex, roles);
                return true;
            };
            PUSH_LAMBDA(update_model2, undo);
            PUSH_LAMBDA(update_model2, redo);
        }
        PUSH_UNDO(undo, redo, i18n("Change clip speed"));
        return true;
    }
    return false;
}
